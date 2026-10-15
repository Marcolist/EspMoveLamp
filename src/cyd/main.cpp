#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFiUdp.h>
#include <XPT2046_Touchscreen.h>
#include <WebServer.h>
#include "library_store.h"
#include "web_ui.h"
#include "scene_icon_assets.h"
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
Lamp::SceneLibrary library = Lamp::defaultLibrary();
WebServer web(80);
uint32_t confirmedSelection = 0, resumeSelection = 1;
bool confirmedParking = false;
IPAddress editorIP(192, 168, 8, 1);
constexpr uint32_t PALETTE[] = {0x000000, 0xffdcaa, 0xffffff, 0xff7828,
                                0xf04460, 0xab60ff, 0x3280ff, 0x24c8aa};
const char* message = "Suche Lampe...";

struct Command {
    bool active = false;
    uint8_t scene = 0, brightness = 0, attempts = 0;
    bool motion = false;
    bool manual = false;
    Lamp::ManualSettings settings;
    uint32_t selection = 0;
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

constexpr uint16_t BG = 0x0845, CARD = 0x1929, TEXT = 0xf79c, MUTED = 0x94b2;
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

// A consistent two-pixel line style, drawn natively at the display resolution.
void iconLine(int x0, int y0, int x1, int y1, uint16_t color) {
    tft.drawLine(x0, y0, x1, y1, color);
    if (abs(x1 - x0) >= abs(y1 - y0)) tft.drawLine(x0, y0 + 1, x1, y1 + 1, color);
    else tft.drawLine(x0 + 1, y0, x1 + 1, y1, color);
}
uint32_t activeSavedId() {
    if (!online || confirmedScene == 0) return 0;
    const auto* s = Lamp::findScene(library, confirmedSelection);
    return s && Lamp::sceneMatches(*s, confirmedScene, confirmedBrightness, confirmedMotion,
                                   confirmedManual, confirmedSettings) ? s->id : 0;
}
uint16_t tileColor(uint32_t rgb, float factor) {
    return tft.color565(uint8_t((rgb >> 16) * factor), uint8_t(((rgb >> 8) & 255) * factor), uint8_t((rgb & 255) * factor));
}
// Composite native-size antialiased sprites onto the exact tile gradient.
// The 64-pixel scanline needs only 128 bytes of RAM; assets stay in flash.
void sceneIcon(uint8_t icon,int cx,int cy,uint32_t tint) {
    if(icon>=SCENE_ICON_COUNT)return;
    uint16_t scanline[SCENE_ICON_SIZE];
    bool swapped=tft.getSwapBytes();
    tft.setSwapBytes(true);
    for(int y=0;y<SCENE_ICON_SIZE;++y) {
        uint16_t background=tileColor(tint,.98f-(43-SCENE_ICON_SIZE/2+y)*.0047f);
        unsigned br=((background>>11)&31)*255/31;
        unsigned bg=((background>>5)&63)*255/63;
        unsigned bb=(background&31)*255/31;
        for(int x=0;x<SCENE_ICON_SIZE;++x) {
            uint32_t pixel=pgm_read_dword(&SCENE_ICON_RGBA[icon][y*SCENE_ICON_SIZE+x]);
            unsigned alpha=pixel>>24, inverse=255-alpha;
            scanline[x]=tft.color565(
                ((((pixel>>16)&255)*alpha+br*inverse+127)/255),
                ((((pixel>>8)&255)*alpha+bg*inverse+127)/255),
                (((pixel&255)*alpha+bb*inverse+127)/255));
        }
        tft.pushImage(cx-SCENE_ICON_SIZE/2,cy-SCENE_ICON_SIZE/2+y,SCENE_ICON_SIZE,1,scanline);
    }
    tft.setSwapBytes(swapped);
}

void sceneTile(const Lamp::SavedScene& scene, int x, int y, bool selected, bool waiting) {
    tft.fillRoundRect(x+2, y+4, 120, 86, 16, 0x0022);
    for (int row=0; row<86; ++row) {
        int edge = 0;
        if (row < 16) edge = 16-lroundf(sqrtf(256-(16-row)*(16-row)));
        else if (row >=70) edge=16-lroundf(sqrtf(256-(row-69)*(row-69)));
        tft.drawFastHLine(x+edge,y+row,120-edge*2,tileColor(scene.tint,.98f-row*.0047f));
    }
    if (selected || waiting) {
        uint16_t ring=waiting ? WARNING : TEXT;
        tft.drawRoundRect(x-2,y-2,124,90,18,ring);
        tft.drawRoundRect(x-1,y-1,122,88,17,ring);
    }
    sceneIcon(scene.icon,x+60,y+43,scene.tint);
    if (selected) {
        tft.fillCircle(x+103,y+14,8,0xffff);
        iconLine(x+99,y+14,x+102,y+17,BG);iconLine(x+102,y+17,x+108,y+11,BG);
    }
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
    bool on = online && confirmedScene != 0;
    int navY = manualTab ? 6 : 202;
    button(224, navY, 84, 32, confirmedParking ? "..." : on ? "AN" : "AUS", pending.active ? WARNING : on ? ACCENT : LINE,
           on ? ACCENT : CARD, on ? BG : TEXT, 2);
    button(12, navY, 98, 32, "Szenen", manualTab ? BG : CARD,
           manualTab ? BG : CARD, manualTab ? MUTED : TEXT);
    button(118, navY, 98, 32, "Manuell", manualTab ? CARD : BG,
           manualTab ? CARD : BG, manualTab ? TEXT : MUTED);
    if (!manualTab) {
        for (uint8_t i = 0; i < 4; ++i) {
            const auto* s = Lamp::findScene(library,library.favorites[i]);
            if (s) sceneTile(*s,24+(i%2)*152,8+(i/2)*96,activeSavedId()==s->id,
                             pending.active && pending.selection==s->id);
        }
        return;
    }
    label("Editor: 192.168.8.1",12,49,1,MUTED,BG);
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
    doc["selection"] = pending.selection;
    Lamp::encodeManual(doc, pending.manual, pending.settings);
    sendDocument(doc, lampIP);
    ++pending.attempts;
    pending.sentAt = millis();
}
bool command(uint8_t scene, uint8_t brightness, bool motion, bool manual,
             const Lamp::ManualSettings& settings, uint32_t selection = 0) {
    if (!online || WiFi.status() != WL_CONNECTED) {
        message = "Lampe nicht erreichbar";
        dirty = true;
        return false; // Never queue a movement command while offline.
    }
    pending.active = true;
    pending.scene = scene;
    pending.brightness = brightness;
    pending.motion = motion;
    pending.manual = manual;
    pending.settings = settings;
    pending.selection = selection;
    pending.attempts = 0;
    pending.sequence = ++sequence;
    transmit();
    dirty = true;
    return true;
}
bool activateScene(uint32_t id) {
    const auto* s = Lamp::findScene(library,id);
    return s && command(s->preset ? s->preset : 1,s->brightness,s->motion,s->preset==0,s->settings,s->id);
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
            !data["ack"].is<bool>() || !data["selection"].is<uint32_t>() ||
            data["selection"].as<uint32_t>() > INT32_MAX || !data["parking"].is<bool>() ||
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
                       confirmedMotion != data["motion"].as<bool>() || confirmedManual != receivedManual ||
                       confirmedSelection != data["selection"].as<uint32_t>() || confirmedParking != data["parking"].as<bool>();
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
        confirmedSelection = data["selection"];
        confirmedParking = data["parking"];
        confirmedSettings = receivedSettings;
        if (confirmedScene != 0) { resumeScene = confirmedScene; resumeSelection = confirmedSelection; }
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
    int navY = manualTab ? 6 : 202;
    if (x >= 224 && x < 308 && y >= navY && y < navY+32) {
        bool stop = confirmedScene != 0 || pending.active;
        command(stop ? 0 : resumeScene, confirmedBrightness, false, confirmedManual, confirmedSettings, stop ? 0 : resumeSelection);
        return;
    }
    if (y >= navY && y < navY+32 && ((x >= 12 && x < 110) || (x >=118 && x<216))) {
        manualTab = x >= 118;
        dirty = true;
        return;
    }
    if (pending.active) return;
    if (!manualTab) {
        for (uint8_t i = 0; i < 4; ++i) {
            int left = 24 + (i % 2) * 152, top = 8 + (i / 2) * 96;
            if (x >= left && x < left + 120 && y >= top && y < top + 86)
                activateScene(library.favorites[i]);
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

void jsonResponse(int code, const JsonDocument& doc) {
    String response;
    serializeJson(doc,response);
    web.sendHeader("Cache-Control","no-store");
    web.send(code,"application/json; charset=utf-8",response);
}
void webError(int code, const char* message) {
    JsonDocument doc; doc["error"]=message; jsonResponse(code,doc);
}
bool requestBody(JsonDocument& doc) {
    if (web.header("X-MoveLamp") != "1") { webError(403,"Bitte den Editor auf diesem CYD verwenden."); return false; }
    if (!web.hasArg("plain") || web.arg("plain").length()>24576) {
        webError(413,"Die Anfrage ist leer oder zu gross (maximal 24 KB).");return false;
    }
    if (deserializeJson(doc,web.arg("plain"),DeserializationOption::NestingLimit(8)) || !doc.is<JsonObject>()) {
        webError(400,"Ungueltige JSON-Datei.");return false;
    }
    return true;
}
void setupWeb() {
    const char* headers[]={"X-MoveLamp"}; web.collectHeaders(headers,1);
    web.on("/",HTTP_GET,[]{
        web.sendHeader("Cache-Control","no-cache");
        web.send_P(200,"text/html; charset=utf-8",WEB_UI);
    });
    web.on("/api/config",HTTP_GET,[]{
        JsonDocument doc;doc["maxScenes"]=Lamp::MAX_SAVED_SCENES;
        doc["minAngle"]=Config::SERVO_MIN_DEG;doc["maxAngle"]=Config::SERVO_MAX_DEG;
        doc["reference"]=Config::SERVO_REFERENCE_DEG;jsonResponse(200,doc);
    });
    web.on("/api/library",HTTP_GET,[]{JsonDocument doc;Lamp::encodeLibrary(doc,library);jsonResponse(200,doc);});
    web.on("/api/library",HTTP_PUT,[]{
        JsonDocument doc;if(!requestBody(doc))return;
        if(!doc["revision"].is<uint32_t>()) {webError(400,"Die Bibliotheksversion fehlt.");return;}
        if(doc["revision"].as<uint32_t>()!=library.revision) {webError(409,"Die Szenen wurden inzwischen geaendert. Bitte neu laden und erneut bearbeiten.");return;}
        if(pending.active) {webError(409,"Bitte die laufende Szenenbestaetigung abwarten.");return;}
        Lamp::SceneLibrary candidate;
        if(!Lamp::decodeLibrary(doc.as<JsonObjectConst>(),candidate)) {
            webError(400,"Ungueltige Szenen: 4 bis 32 eindeutige Szenen, 4 verschiedene Favoriten, Namen bis 32 Bytes und gueltige Farben/Winkel erforderlich.");return;
        }
        candidate.revision=library.revision+1;
        if(!LibraryStore::save(candidate)) {webError(507,"Speichern fehlgeschlagen. Die bisherige Bibliothek bleibt erhalten.");return;}
        library=candidate;dirty=true;
        JsonDocument response;Lamp::encodeLibrary(response,library);jsonResponse(200,response);
    });
    web.on("/api/status",HTTP_GET,[]{
        JsonDocument doc;doc["online"]=online;doc["pending"]=pending.active;
        doc["active"]=activeSavedId();doc["selection"]=confirmedSelection;
        doc["parking"]=online&&confirmedParking;doc["revision"]=library.revision;
        doc["address"]=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():editorIP.toString();
        doc["heap"]=ESP.getFreeHeap();jsonResponse(200,doc);
    });
    web.on("/api/activate",HTTP_POST,[]{
        JsonDocument doc;if(!requestBody(doc))return;
        if(!doc["id"].is<uint32_t>()||!Lamp::findScene(library,doc["id"])) {webError(404,"Szene nicht gefunden.");return;}
        if(!online) {webError(503,"Die Lampe ist noch nicht verbunden.");return;}
        if(pending.active) {webError(409,"Eine Szene wird bereits eingestellt.");return;}
        if(!activateScene(doc["id"])) {webError(503,"Die Lampe ist nicht erreichbar.");return;}
        JsonDocument response;response["pending"]=true;jsonResponse(202,response);
    });
    web.on("/api/power",HTTP_POST,[]{
        JsonDocument doc;if(!requestBody(doc))return;
        if(!doc["on"].is<bool>()||doc["on"].as<bool>()) {webError(400,"Zum Einschalten bitte eine Szene auswaehlen.");return;}
        if(!command(0,confirmedBrightness,false,confirmedManual,confirmedSettings)) {webError(503,"Die Lampe ist nicht erreichbar.");return;}
        JsonDocument response;response["pending"]=true;jsonResponse(202,response);
    });
    web.onNotFound([]{webError(404,"Nicht gefunden.");});
    web.begin();
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
    if(!LibraryStore::load(library)) {
        library=Lamp::defaultLibrary();
        if(!LibraryStore::save(library)) Serial.println("Scene defaults in RAM; storage not available");
    }
    // Separate subnet avoids conflicting with the lamp's 192.168.4.1 fallback AP.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(editorIP,editorIP,IPAddress(255,255,255,0));
    if(!WiFi.softAP("MoveLamp-Setup","MoveLamp1234")) Serial.println("Editor AP failed");
    setupWeb();
    Serial.println("Editor: MoveLamp-Setup / http://192.168.8.1");
    loadCalibration();
}

void loop() {
    uint32_t now = millis();
    network.tick();
    web.handleClient();
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
