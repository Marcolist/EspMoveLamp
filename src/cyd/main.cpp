#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFiUdp.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"
#include "network.h"
#include "protocol.h"
#include "scenes.h"

namespace {
TFT_eSPI tft;
// TFT uses HSPI; touch gets its own hardware SPI peripheral.
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(Config::TOUCH_CS_PIN, Config::TOUCH_IRQ_PIN);
Network network(false);
WiFiUDP udp;
bool udpReady = false, online = false, dirty = true;
IPAddress lampIP;
uint32_t session, sequence = 0, lampBoot = 0;
uint32_t lastSeen = 0, lastDiscover = 0, lastPaint = 0;
uint8_t confirmedScene = 0, confirmedBrightness = Config::INITIAL_BRIGHTNESS;
bool confirmedMotion = false;
bool confirmedManual = false, manualTab = false, draftEdited = false;
Lamp::ManualSettings confirmedSettings, draft;
uint8_t draftBrightness = Config::INITIAL_BRIGHTNESS, selectedSide = 0, resumeScene = 1;
bool draftMotion = false;
constexpr uint8_t PRESETS[] = {1, 2, 4, 6};
constexpr const char* PRESET_NAMES[] = {"Warm", "Lesen", "Ozean", "Kerze"};
constexpr const char* PRESET_DETAILS[] = {"Ankommen & entspannen", "Klar & fokussiert", "Sanfte blaue Wellen", "Warmes Flackern"};
constexpr uint32_t PALETTE[] = {0x000000, 0xffdcaa, 0xffffff, 0xff7828,
                                0xf04460, 0xab60ff, 0x3280ff, 0x24c8aa};
const char* message = "Suche Lampe...";

struct Command {
    bool active = false;
    uint8_t scene = 0, brightness = 0, attempts = 0;
    bool motion = false;
    bool manual = false;
    Lamp::ManualSettings settings;
    uint32_t sequence = 0, sentAt = 0;
} pending;

struct Calibration {
    uint32_t version;
    float ax, bx, cx, ay, by, cy;
} calibration{};
bool calibrating = false, calibrationRelease = true;
uint8_t calibrationPoint = 0;
int32_t rawX[3], rawY[3];
int32_t calibrationSumX = 0, calibrationSumY = 0;
uint16_t calibrationSamples = 0;
constexpr int16_t targetX[3] = {20, 300, 20}, targetY[3] = {20, 20, 220};
bool wasPressed = false;
uint32_t pressedAt = 0;
uint32_t releasedAt = 0;

constexpr uint16_t BG = 0x1082, CARD = 0x2124, TEXT = 0xf79c, MUTED = 0x94b2;
constexpr uint16_t ACCENT = 0xa6b4, WARNING = 0xfd20;
constexpr uint16_t LINE = 0x39c7;

uint16_t rgb565(uint32_t rgb) {
    return tft.color565(rgb >> 16, rgb >> 8, rgb);
}

void label(const String& text, int x, int y, uint8_t font, uint16_t color, uint16_t bg) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(color, bg);
    tft.drawString(text, x, y, font);
}
void button(int x, int y, int w, int h, const String& text, uint16_t border,
            uint16_t fill = CARD, uint16_t ink = TEXT, uint8_t font = 2) {
    tft.fillRoundRect(x, y, w, h, 7, fill);
    tft.drawRoundRect(x, y, w, h, 7, border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ink, fill);
    tft.drawString(text, x + w / 2, y + h / 2, font);
}

void drawCalibration() {
    tft.fillScreen(BG);
    label("Touch einrichten", 62, 75, 4, TEXT, BG);
    label("Kreuz kurz halten, dann loslassen", 30, 112, 2, MUTED, BG);
    label(String(calibrationPoint + 1) + " / 3", 144, 138, 2, ACCENT, BG);
    int x = targetX[calibrationPoint], y = targetY[calibrationPoint];
    tft.drawCircle(x, y, 10, ACCENT);
    tft.drawFastHLine(x - 14, y, 29, TEXT);
    tft.drawFastVLine(x, y - 14, 29, TEXT);
}
void beginCalibration() {
    calibrating = true;
    calibrationRelease = true;
    calibrationPoint = 0;
    drawCalibration();
}
bool solveCalibration() {
    const float ux = rawX[1] - rawX[0], uy = rawY[1] - rawY[0];
    const float vx = rawX[2] - rawX[0], vy = rawY[2] - rawY[0];
    const float determinant = ux * vy - uy * vx;
    if (fabsf(determinant) < 100000) return false;
    calibration.version = 1;
    calibration.ax = 280 * vy / determinant;
    calibration.bx = -280 * vx / determinant;
    calibration.cx = 20 - calibration.ax * rawX[0] - calibration.bx * rawY[0];
    calibration.ay = -200 * uy / determinant;
    calibration.by = 200 * ux / determinant;
    calibration.cy = 20 - calibration.ay * rawX[0] - calibration.by * rawY[0];
    Preferences prefs;
    prefs.begin("move-touch", false);
    prefs.putBytes("affine", &calibration, sizeof(calibration));
    prefs.end();
    return true;
}
void loadCalibration() {
    Preferences prefs;
    prefs.begin("move-touch", true);
    bool loaded = prefs.getBytesLength("affine") == sizeof(calibration);
    if (loaded) prefs.getBytes("affine", &calibration, sizeof(calibration));
    prefs.end();
    bool finite = isfinite(calibration.ax) && isfinite(calibration.bx) && isfinite(calibration.cx) &&
                  isfinite(calibration.ay) && isfinite(calibration.by) && isfinite(calibration.cy);
    if (!loaded || calibration.version != 1 || !finite || digitalRead(0) == LOW) beginCalibration();
}

void drawUI() {
    tft.fillScreen(BG);
    label("MOVE", 12, 4, 4, TEXT, BG);
    tft.fillCircle(14, 33, 2, pending.active ? WARNING : online ? ACCENT : MUTED);
    label(pending.active ? "Wird eingestellt" : online ? "Verbunden" : "Lampe suchen", 22, 29, 1, MUTED, BG);
    bool on = online && confirmedScene != 0;
    button(224, 6, 84, 32, on ? "AN" : "AUS", pending.active ? WARNING : on ? ACCENT : LINE,
           on ? ACCENT : CARD, on ? BG : TEXT, 2);
    button(12, 46, 144, 26, "Presets", manualTab ? BG : CARD,
           manualTab ? BG : CARD, manualTab ? MUTED : TEXT);
    button(164, 46, 144, 26, "Manuell", manualTab ? CARD : BG,
           manualTab ? CARD : BG, manualTab ? TEXT : MUTED);
    if (!manualTab) {
        for (uint8_t i = 0; i < 4; ++i) {
            int x = 12 + (i % 2) * 152, y = 82 + (i / 2) * 73;
            bool waiting = pending.active && !pending.manual && pending.scene == PRESETS[i];
            bool selected = online && !confirmedManual && confirmedScene == PRESETS[i];
            uint16_t edge = waiting ? WARNING : selected ? ACCENT : CARD;
            tft.fillRoundRect(x, y, 144, 65, 8, CARD);
            tft.drawRoundRect(x, y, 144, 65, 8, edge);
            tft.fillRoundRect(x + 12, y + 10, 24, 3, 1, Lamp::SCENES[PRESETS[i]].accent);
            label(PRESET_NAMES[i], x + 12, y + 20, 4, TEXT, CARD);
            label(PRESET_DETAILS[i], x + 12, y + 49, 1, MUTED, CARD);
            if (selected) tft.fillCircle(x + 130, y + 12, 3, ACCENT);
        }
        label(online ? (confirmedManual && on ? "Manuelles Licht aktiv" : on ? "Dein Licht. Deine Stimmung." : "Bereit, wenn du es bist.")
                     : "Warte auf die Lampe...", 12, 229, 1, MUTED, BG);
        return;
    }
    const char* sides[] = {"Oben", "Unten", "Vorne"};
    for (uint8_t i = 0; i < 3; ++i) {
        button(12 + i * 100, 79, 96, 28, sides[i], selectedSide == i ? ACCENT : CARD);
        tft.fillCircle(24 + i * 100, 93, 4, rgb565(draft.colors[i]));
    }
    for (uint8_t i = 0; i < 8; ++i) {
        int x = 12 + i * 38;
        uint16_t color = rgb565(PALETTE[i]);
        tft.fillRoundRect(x, 114, 30, 28, 5, color);
        if (i == 0) tft.drawLine(x + 8, 134, x + 22, 121, MUTED);
        if (draft.colors[selectedSide] == PALETTE[i]) {
            tft.drawRoundRect(x - 2, 112, 34, 32, 6, TEXT);
        }
    }
    label("Winkel", 12, 156, 2, MUTED, BG);
    button(112, 151, 38, 29, "-", CARD);
    button(270, 151, 38, 29, "+", CARD);
    label(String(int(lroundf(draft.target))) + " Grad", 174, 156, 2, TEXT, BG);
    label("Helligkeit", 12, 190, 2, MUTED, BG);
    button(112, 185, 38, 26, "-", CARD);
    button(270, 185, 38, 26, "+", CARD);
    label(String((unsigned(draftBrightness) * 100 + 127) / 255) + " %", 180, 190, 2, TEXT, BG);
    button(12, 217, 144, 23, draftMotion ? "Servo an" : "Servo aus", draftMotion ? ACCENT : LINE);
    button(164, 217, 144, 23, pending.active ? "Wird gesendet" : "Anwenden", ACCENT, ACCENT, BG);
}

void sendDocument(JsonDocument& doc, IPAddress ip) {
    udp.beginPacket(ip, Config::UDP_PORT);
    serializeJson(doc, udp);
    udp.endPacket();
}
void baseDocument(JsonDocument& doc, const char* type) {
    doc["v"] = Lamp::PROTOCOL_VERSION;
    doc["key"] = LAMP_PAIRING_KEY;
    doc["type"] = type;
}
void discover(uint32_t now) {
    lastDiscover = now;
    JsonDocument doc;
    baseDocument(doc, "discover");
    if (online) sendDocument(doc, lampIP);
    else {
        IPAddress configured;
        if (configured.fromString(Config::LAMP_IP)) sendDocument(doc, configured);
        IPAddress broadcast = (uint32_t(WiFi.localIP()) & uint32_t(WiFi.subnetMask())) |
                              ~uint32_t(WiFi.subnetMask());
        sendDocument(doc, broadcast);
        // The lamp is also the gateway when using its fallback AP.
        if (network.fallback()) sendDocument(doc, WiFi.gatewayIP());
    }
}
void transmit() {
    JsonDocument doc;
    baseDocument(doc, "set");
    doc["session"] = session;
    doc["boot"] = lampBoot;
    doc["seq"] = pending.sequence;
    doc["scene"] = pending.scene;
    doc["brightness"] = pending.brightness;
    doc["motion"] = pending.motion;
    Lamp::encodeManual(doc, pending.manual, pending.settings);
    sendDocument(doc, lampIP);
    ++pending.attempts;
    pending.sentAt = millis();
}
void command(uint8_t scene, uint8_t brightness, bool motion, bool manual,
             const Lamp::ManualSettings& settings) {
    if (!online || WiFi.status() != WL_CONNECTED) {
        message = "Lampe nicht erreichbar";
        dirty = true;
        return; // Never queue a movement command while offline.
    }
    pending.active = true;
    pending.scene = scene;
    pending.brightness = brightness;
    pending.motion = motion;
    pending.manual = manual;
    pending.settings = settings;
    pending.attempts = 0;
    pending.sequence = ++sequence;
    transmit();
    dirty = true;
}
void receive(uint32_t now) {
    for (int n = 0; n < 4; ++n) {
        int size = udp.parsePacket();
        if (!size) return;
        if (size >= int(Config::PACKET_BYTES) || udp.remotePort() != Config::UDP_PORT) {
            udp.flush(); continue;
        }
        IPAddress from = udp.remoteIP();
        if (online && from != lampIP) { udp.flush(); continue; }
        char buffer[Config::PACKET_BYTES];
        int len = udp.read(buffer, sizeof(buffer) - 1);
        if (len <= 0) continue;
        JsonDocument doc;
        if (deserializeJson(doc, buffer, len)) continue;
        JsonObjectConst data = doc.as<JsonObjectConst>();
        bool receivedManual;
        Lamp::ManualSettings receivedSettings;
        if (!Lamp::validEnvelope(data) ||
            data["type"] != "status" || data["device"] != Lamp::DEVICE_ID ||
            !data["session"].is<uint32_t>() || !data["seq"].is<uint32_t>() ||
            !data["ack"].is<bool>() ||
            !data["boot"].is<uint32_t>() || !data["scene"].is<unsigned int>() ||
            !data["brightness"].is<unsigned int>() || !data["motion"].is<bool>() ||
            data["scene"].as<unsigned int>() >= Lamp::SCENE_COUNT ||
            data["brightness"].as<unsigned int>() > 255 ||
            !Lamp::decodeManual(data, receivedManual, receivedSettings)) continue;
        // Status from an older command must not overwrite a newer acknowledged state.
        bool heartbeat = data["session"] == 0 && data["seq"] == 0;
        bool matching = pending.active && data["session"] == session && data["seq"] == pending.sequence;
        if (!heartbeat && !matching) continue;
        bool changed = !online || confirmedScene != data["scene"].as<uint8_t>() ||
                       confirmedBrightness != data["brightness"].as<uint8_t>() ||
                       confirmedMotion != data["motion"].as<bool>() || confirmedManual != receivedManual;
        const char* previousMessage = message;
        uint32_t boot = data["boot"];
        if (pending.active && lampBoot && boot != lampBoot) {
            pending.active = false;
            message = "Lampe neu gestartet";
            changed = true;
        } else if (matching) {
            pending.active = false;
            if (data["ack"] == true && pending.manual) draftEdited = false;
            message = data["ack"] == true ? "Bestaetigt" : "Bitte erneut waehlen";
            changed = true;
        } else if (!pending.active) message = "Bereit";
        lampBoot = boot;
        lampIP = from;
        lastSeen = now;
        online = true;
        confirmedScene = data["scene"];
        confirmedBrightness = data["brightness"];
        confirmedMotion = data["motion"];
        confirmedManual = receivedManual;
        confirmedSettings = receivedSettings;
        if (confirmedScene != 0) resumeScene = confirmedScene;
        if (!draftEdited) {
            draft = confirmedSettings;
            draftBrightness = confirmedBrightness;
            draftMotion = confirmedMotion;
        }
        dirty = dirty || changed || previousMessage != message;
    }
}
void handleTap(int x, int y) {
    // Power off supersedes a pending command and disables the servo as well.
    if (x >= 224 && x < 308 && y >= 6 && y < 38) {
        bool stop = confirmedScene != 0 || pending.active;
        command(stop ? 0 : resumeScene, confirmedBrightness, false, confirmedManual, confirmedSettings);
        return;
    }
    if (y >= 46 && y < 72 && x >= 12 && x < 308 && (x < 156 || x >= 164)) {
        manualTab = x >= 164;
        dirty = true;
        return;
    }
    if (pending.active) return;
    if (!manualTab) {
        for (uint8_t i = 0; i < 4; ++i) {
            int left = 12 + (i % 2) * 152, top = 82 + (i / 2) * 73;
            if (x >= left && x < left + 144 && y >= top && y < top + 65)
                command(PRESETS[i], confirmedBrightness, confirmedMotion, false, confirmedSettings);
        }
        return;
    }
    if (y >= 79 && y < 107) {
        for (uint8_t i = 0; i < 3; ++i)
            if (x >= 12 + i * 100 && x < 108 + i * 100) selectedSide = i;
    } else if (y >= 114 && y < 142) {
        for (uint8_t i = 0; i < 8; ++i) {
            if (x >= 12 + i * 38 && x < 42 + i * 38) {
                draft.colors[selectedSide] = PALETTE[i];
                draftEdited = true;
            }
        }
    } else if (y >= 151 && y < 180) {
        int delta = x >= 112 && x < 150 ? -1 : x >= 270 && x < 308 ? 1 : 0;
        draft.target = Lamp::clamp(draft.target + delta, Config::SERVO_MIN_DEG, Config::SERVO_MAX_DEG);
        if (delta) draftEdited = true;
    } else if (y >= 185 && y < 211) {
        int delta = x >= 112 && x < 150 ? -26 : x >= 270 && x < 308 ? 26 : 0;
        draftBrightness = constrain(int(draftBrightness) + delta, 0, 255);
        if (delta) draftEdited = true;
    } else if (y >= 217 && y < 240) {
        if (x >= 12 && x < 156) { draftMotion = !draftMotion; draftEdited = true; }
        if (x >= 164 && x < 308) {
            command(1, draftBrightness, draftMotion, true, draft);
            // Keep the draft until a matching acknowledgement arrives.
        }
    }
    dirty = true;
}

void tickTouch(uint32_t now) {
    bool pressed = touch.touched();
    TS_Point point;
    if (pressed) {
        point = touch.getPoint();
        pressed = point.z >= Config::TOUCH_PRESSURE_MIN;
    }
    if (calibrating) {
        if (calibrationRelease) {
            if (!pressed) { calibrationRelease = false; wasPressed = false; }
            return;
        }
        if (pressed && !wasPressed) {
            pressedAt = now;
            calibrationSumX = calibrationSumY = 0;
            calibrationSamples = 0;
        }
        if (pressed) {
            calibrationSumX += point.x;
            calibrationSumY += point.y;
            ++calibrationSamples;
        }
        if (pressed && wasPressed && Lamp::due(now, pressedAt, 180)) {
            // Samples are collected on separate loop iterations, beyond the
            // touch library's internal 3ms sample cache.
            rawX[calibrationPoint] = calibrationSumX / calibrationSamples;
            rawY[calibrationPoint] = calibrationSumY / calibrationSamples;
            ++calibrationPoint;
            calibrationRelease = true;
            if (calibrationPoint == 3) {
                if (solveCalibration()) {
                    calibrating = false;
                    dirty = true;
                    // Keep pressed latched until release to avoid a phantom scene tap.
                } else beginCalibration();
            } else drawCalibration();
        }
    } else if (!pressed) {
        if (wasPressed && !releasedAt) releasedAt = now;
        if (Lamp::due(now, releasedAt, 90)) wasPressed = false;
        return;
    } else if (!wasPressed) {
        int x = lroundf(calibration.ax * point.x + calibration.bx * point.y + calibration.cx);
        int y = lroundf(calibration.ay * point.x + calibration.by * point.y + calibration.cy);
        if (x >= 0 && x < 320 && y >= 0 && y < 240) handleTap(x, y);
    }
    releasedAt = 0;
    wasPressed = pressed;
}
}

void setup() {
    Serial.begin(115200);
    pinMode(0, INPUT_PULLUP);
    for (int pin : {4, 16, 17}) { pinMode(pin, OUTPUT); digitalWrite(pin, HIGH); }
    tft.init();
    tft.setRotation(Config::TFT_ROTATION);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL, 0);
    ledcWrite(0, Config::DISPLAY_BACKLIGHT);
    touchSPI.begin(Config::TOUCH_CLK_PIN, Config::TOUCH_MISO_PIN,
                   Config::TOUCH_MOSI_PIN, Config::TOUCH_CS_PIN);
    touch.begin(touchSPI);
    touch.setRotation(Config::TOUCH_ROTATION);
    session = esp_random();
    if (!session) session = 1;
    network.begin();
    loadCalibration();
}

void loop() {
    uint32_t now = millis();
    network.tick();
    if (WiFi.status() == WL_CONNECTED) {
        if (!udpReady) udpReady = udp.begin(Config::UDP_PORT + 1);
        if (udpReady) {
            receive(now);
            if (Lamp::due(now, lastDiscover, Config::DISCOVERY_MS)) discover(now);
            if (pending.active && Lamp::due(now, pending.sentAt, Config::ACK_TIMEOUT_MS)) {
                if (pending.attempts < Config::MAX_ATTEMPTS) transmit();
                else {
                    pending.active = false;
                    message = "Keine Bestaetigung";
                    dirty = true;
                }
            }
        }
    } else {
        if (udpReady) { udp.stop(); udpReady = false; }
        if (online || pending.active) {
            online = false;
            pending.active = false;
            message = "WLAN getrennt";
            dirty = true;
        }
    }
    if (online && Lamp::due(now, lastSeen, Config::OFFLINE_MS)) {
        online = false;
        pending.active = false;
        message = "Lampe nicht erreichbar";
        dirty = true;
    }
    tickTouch(now);
    if (!calibrating && dirty && Lamp::due(now, lastPaint, 100)) {
        drawUI(); dirty = false; lastPaint = now;
    }
    delay(8);
}
