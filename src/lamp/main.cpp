#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiUdp.h>
#include "config.h"
#include "network.h"
#include "protocol.h"
#include "scenes.h"
#include "motion.h"

namespace {
Network network(true);
WiFiUDP udp;
Adafruit_NeoPixel strips[Config::LED_OUTPUTS];
Lamp::RGB frame[Config::LED_COUNT];
uint8_t sceneId = 0, brightness = Config::INITIAL_BRIGHTNESS;
bool motion = false, servoAttached = false, udpReady = false;
bool manual = false;
Lamp::ReferenceReturn parking;
Lamp::ManualSettings manualSettings;
float angle = Config::SERVO_HOME_DEG;
uint32_t sceneStarted = 0, servoTick = 0, ledTick = 0;
Lamp::CommandGate commandGate;
uint8_t appliedBrightness = 0;
uint32_t bootId;
uint32_t selectionId = 0;

void writeServo() {
    float degrees = Config::SERVO_REVERSED ? Config::SERVO_RANGE_DEG - angle : angle;
    int pulse = lroundf(Config::SERVO_MIN_US + degrees / Config::SERVO_RANGE_DEG *
                        (Config::SERVO_MAX_US - Config::SERVO_MIN_US));
    ledcWrite(Config::SERVO_PWM_CHANNEL, uint32_t(pulse) * 65535UL / 20000UL);
}

void updateServo(uint32_t now) {
    if (!Lamp::due(now, servoTick, 20)) return;
    // Cap dt so delayed network processing cannot create a large position jump.
    float dt = fminf(float(uint32_t(now - servoTick)) / 1000, .05f);
    servoTick = now;
    if (!motion && !parking.active()) {
        if (servoAttached) {
            ledcDetachPin(Config::SERVO_PIN);
            pinMode(Config::SERVO_PIN, OUTPUT);
            digitalWrite(Config::SERVO_PIN, LOW);
            servoAttached = false;
        }
        return;
    }
    if (!servoAttached) {
        ledcSetup(Config::SERVO_PWM_CHANNEL, 50, 16);
        writeServo();
        ledcAttachPin(Config::SERVO_PIN, Config::SERVO_PWM_CHANNEL);
        servoAttached = true;
    }
    if (sceneId != 0 || parking.active()) {
        const float target = parking.active() ? Config::SERVO_REFERENCE_DEG : manual ? manualSettings.target : Lamp::targetAngle(Lamp::SCENES[sceneId], now - sceneStarted,
                                              Config::SERVO_MIN_DEG, Config::SERVO_MAX_DEG);
        angle = Lamp::approach(angle, target, Config::SERVO_SPEED_DPS * dt);
    }
    writeServo();
    parking.tick(now,angle);
}

void updateLEDs(uint32_t now) {
    if (!Lamp::due(now, ledTick, 25)) return;
    ledTick = now;
    uint32_t channelSum = 0;
    for (uint8_t s = 0; s < Config::SECTIONS; ++s) {
        for (uint8_t r = 0; r < Config::ROWS_PER_SECTION; ++r) {
            for (uint16_t p = 0; p < Config::LEDS_PER_ROW; ++p) {
                auto c = sceneId != 0 && manual ? Lamp::manualColor(manualSettings, s) :
                         Lamp::color(Lamp::SCENES[sceneId].effect, now - sceneStarted,
                                     s, r, p, Config::LEDS_PER_ROW);
                uint16_t physical = Config::REVERSE_STRIP[s] ? Config::LEDS_PER_ROW - 1 - p : p;
                uint16_t index = Lamp::pixelIndex(s, r, physical, Config::ROWS_PER_SECTION,
                                                 Config::LEDS_PER_ROW, Config::SERPENTINE_ROWS);
                frame[index] = c;
                channelSum += c.r + c.g + c.b;
            }
        }
    }
    // Conservative classic WS2812 estimate: 20mA per full-intensity channel.
    appliedBrightness = Lamp::limitedBrightness(channelSum, brightness, Config::LED_CURRENT_LIMIT_MA);
    constexpr uint16_t perOutput = Config::LED_COUNT / Config::LED_OUTPUTS;
    for (uint16_t i = 0; i < Config::LED_COUNT; ++i) {
        auto c = frame[i];
        strips[i / perOutput].setPixelColor(i % perOutput,
            uint16_t(c.r) * appliedBrightness / 255,
            uint16_t(c.g) * appliedBrightness / 255,
            uint16_t(c.b) * appliedBrightness / 255);
    }
    for (auto& strip : strips) strip.show();
}

void reply(IPAddress ip, uint16_t port, uint32_t session, uint32_t seq, bool ack) {
    JsonDocument doc;
    doc["v"] = Lamp::PROTOCOL_VERSION;
    doc["type"] = "status";
    doc["device"] = Lamp::DEVICE_ID;
    doc["key"] = LAMP_PAIRING_KEY;
    doc["boot"] = bootId;
    doc["session"] = session;
    doc["seq"] = seq;
    doc["ack"] = ack;
    doc["scene"] = sceneId;
    doc["selection"] = selectionId;
    doc["brightness"] = brightness;
    doc["applied"] = appliedBrightness;
    doc["motion"] = motion;
    doc["parking"] = parking.active();
    Lamp::encodeManual(doc, manual, manualSettings);
    doc["angle"] = angle; // commanded angle, no position sensor
    doc["leds"] = Config::LED_COUNT;
    udp.beginPacket(ip, port);
    serializeJson(doc, udp);
    udp.endPacket();
}

void receive() {
    // Bounded processing keeps servo and LED updates running under heavy traffic.
    for (int n = 0; n < 4; ++n) {
        int size = udp.parsePacket();
        if (!size) return;
        IPAddress ip = udp.remoteIP();
        uint16_t port = udp.remotePort();
        if (size >= int(Config::PACKET_BYTES)) { udp.flush(); continue; }
        char buffer[Config::PACKET_BYTES];
        int len = udp.read(buffer, sizeof(buffer) - 1);
        if (len <= 0) continue;
        JsonDocument doc;
        if (deserializeJson(doc, buffer, len)) continue;
        JsonObjectConst data = doc.as<JsonObjectConst>();
        if (!Lamp::validEnvelope(data)) continue;
        if (data["type"] == "discover") { reply(ip, port, 0, 0, false); continue; }
        Lamp::Command cmd;
        if (!Lamp::decodeCommand(data, bootId, cmd)) continue;
        auto order = commandGate.inspect(cmd.session, cmd.seq);
        bool duplicate = order == Lamp::CommandOrder::Duplicate;
        bool accept = order == Lamp::CommandOrder::New;
        if (accept) {
            if (sceneId != cmd.scene) sceneStarted = millis();
            sceneId = cmd.scene;
            selectionId = cmd.selection;
            brightness = cmd.brightness;
            motion = cmd.scene != 0 && cmd.motion;
            if (cmd.scene == 0) parking.start(); else parking.cancel();
            manual = cmd.manual;
            manualSettings = cmd.settings;
            Serial.printf("Scene %u, brightness %u, motion %s\n", sceneId, brightness, motion ? "on" : "off");
        }
        // Duplicate retries acknowledge without restarting the animation.
        reply(ip, port, cmd.session, cmd.seq, accept || duplicate);
    }
}
}

void setup() {
    Serial.begin(115200);
    bootId = esp_random();
    pinMode(Config::SERVO_PIN, OUTPUT);
    digitalWrite(Config::SERVO_PIN, LOW);
    constexpr uint16_t perOutput = Config::LED_COUNT / Config::LED_OUTPUTS;
    for (uint8_t i = 0; i < Config::LED_OUTPUTS; ++i) {
        strips[i].updateType(NEO_GRB + NEO_KHZ800);
        strips[i].updateLength(perOutput);
        strips[i].setPin(Config::LED_PINS[i]);
        strips[i].begin();
        strips[i].clear();
        strips[i].show();
    }
    network.begin();
    Serial.printf("EspMoveLamp: %u LEDs, boot OFF, servo disabled\n", Config::LED_COUNT);
}

void loop() {
    network.tick();
    if (!udpReady && network.ready()) udpReady = udp.begin(Config::UDP_PORT);
    if (udpReady) receive();
    uint32_t now = millis();
    updateServo(now);
    updateLEDs(now);
    delay(1);
}
