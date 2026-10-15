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
const char* message = "Suche Lampe...";

struct Command {
    bool active = false;
    uint8_t scene = 0, brightness = 0, attempts = 0;
    bool motion = false;
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

constexpr uint16_t BG = 0x0842, CARD = 0x18e4, TEXT = 0xef7d, MUTED = 0x8c71;
constexpr uint16_t ACCENT = 0x05f5, WARNING = 0xfd20;

void label(const String& text, int x, int y, uint8_t font, uint16_t color, uint16_t bg) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(color, bg);
    tft.drawString(text, x, y, font);
}
void button(int x, int y, int w, int h, const String& text, uint16_t border) {
    tft.fillRoundRect(x, y, w, h, 4, CARD);
    tft.drawRoundRect(x, y, w, h, 4, border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TEXT, CARD);
    tft.drawString(text, x + w / 2, y + h / 2, 2);
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
    label("MOVE / LAMP", 8, 2, 2, TEXT, BG);
    label(pending.active ? "SENDET..." : online ? "VERBUNDEN" : "OFFLINE",
          217, 2, 2, pending.active ? WARNING : online ? ACCENT : MUTED, BG);
    String status = WiFi.status() == WL_CONNECTED ?
        (online ? lampIP.toString() + "  " + message : String(message)) : "WLAN verbindet...";
    label(status, 8, 23, 1, MUTED, BG);
    for (uint8_t i = 0; i < Lamp::SCENE_COUNT; ++i) {
        int x = 8 + (i % 2) * 156, y = 36 + (i / 2) * 34;
        bool waiting = pending.active && pending.scene == i;
        uint16_t edge = waiting ? WARNING : confirmedScene == i && online ? ACCENT : CARD;
        button(x, y, 148, 29, String(Lamp::SCENES[i].name) + (waiting ? " ..." : ""), edge);
        tft.fillCircle(x + 10, y + 14, 3, Lamp::SCENES[i].accent);
    }
    button(8, 176, 38, 27, "-", MUTED);
    button(274, 176, 38, 27, "+", MUTED);
    const uint8_t level = pending.active ? pending.brightness : confirmedBrightness;
    label("Licht " + String((unsigned(level) * 100 + 127) / 255) + "%", 115, 181, 2, TEXT, BG);
    button(8, 209, 148, 25, confirmedMotion ? "Servo: AN" : "Servo: AUS", confirmedMotion ? ACCENT : MUTED);
    button(164, 209, 148, 25, "ALLES STOPP", 0xf800);
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
    sendDocument(doc, lampIP);
    ++pending.attempts;
    pending.sentAt = millis();
}
void command(uint8_t scene, uint8_t brightness, bool motion) {
    if (!online || WiFi.status() != WL_CONNECTED) {
        message = "Lampe nicht erreichbar";
        dirty = true;
        return; // Never queue a movement command while offline.
    }
    pending.active = true;
    pending.scene = scene;
    pending.brightness = brightness;
    pending.motion = motion;
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
        if (!Lamp::validEnvelope(data) ||
            data["type"] != "status" || data["device"] != Lamp::DEVICE_ID ||
            !data["session"].is<uint32_t>() || !data["seq"].is<uint32_t>() ||
            !data["ack"].is<bool>() ||
            !data["boot"].is<uint32_t>() || !data["scene"].is<unsigned int>() ||
            !data["brightness"].is<unsigned int>() || !data["motion"].is<bool>() ||
            data["scene"].as<unsigned int>() >= Lamp::SCENE_COUNT ||
            data["brightness"].as<unsigned int>() > 255) continue;
        // Status from an older command must not overwrite a newer acknowledged state.
        bool heartbeat = data["session"] == 0 && data["seq"] == 0;
        bool matching = pending.active && data["session"] == session && data["seq"] == pending.sequence;
        if (!heartbeat && !matching) continue;
        bool changed = !online || confirmedScene != data["scene"].as<uint8_t>() ||
                       confirmedBrightness != data["brightness"].as<uint8_t>() ||
                       confirmedMotion != data["motion"].as<bool>();
        const char* previousMessage = message;
        uint32_t boot = data["boot"];
        if (pending.active && lampBoot && boot != lampBoot) {
            pending.active = false;
            message = "Lampe neu gestartet";
            changed = true;
        } else if (matching) {
            pending.active = false;
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
        dirty = dirty || changed || previousMessage != message;
    }
}
void handleTap(int x, int y) {
    // Stop can supersede an unacknowledged command; the lamp rejects its older retries.
    if (y >= 209 && y < 234 && x >= 164 && x < 312) {
        command(0, confirmedBrightness, false);
        return;
    }
    if (pending.active) return;
    if (y >= 36 && y < 167) {
        int row = (y - 36) / 34;
        int col = x >= 164 ? 1 : 0;
        int left = 8 + col * 156;
        if (x >= left && x < left + 148 && (y - 36) % 34 < 29) {
            command(row * 2 + col, confirmedBrightness, confirmedMotion);
        }
    } else if (y >= 176 && y < 203) {
        if (x >= 8 && x < 46) command(confirmedScene, max(0, int(confirmedBrightness) - 26), confirmedMotion);
        if (x >= 274 && x < 312) command(confirmedScene, min(255, int(confirmedBrightness) + 26), confirmedMotion);
    } else if (y >= 209 && y < 234 && x >= 8 && x < 156) {
        command(confirmedScene, confirmedBrightness, !confirmedMotion);
    }
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
