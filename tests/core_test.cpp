#include <assert.h>
#include <iostream>
#include <set>
#include "protocol.h"

void testMapping() {
    static_assert(Config::LED_COUNT == 120, "Hardware has 120 LEDs");
    for (bool serpentine : {false, true}) {
        std::set<uint16_t> seen;
        for (unsigned s = 0; s < 4; ++s)
            for (unsigned r = 0; r < 3; ++r)
                for (unsigned p = 0; p < 30; ++p)
                    seen.insert(Lamp::pixelIndex(s, r, p, 3, 30, serpentine));
        assert(seen.size() == 360 && *seen.begin() == 0 && *seen.rbegin() == 359);
    }
    std::set<uint16_t> seen;
    for (unsigned s = 0; s < 4; ++s)
        for (unsigned p = 0; p < 30; ++p)
            seen.insert(Lamp::pixelIndex(s, 0, p, 1, 30, false));
    assert(seen.size() == 120 && *seen.rbegin() == 119);
}
void testMotionAndEffects() {
    for (auto& scene : Lamp::SCENES) {
        for (uint32_t time = 0; time < 90000; time += 137) {
            auto target = Lamp::targetAngle(scene, time, Config::SERVO_MIN_DEG, Config::SERVO_MAX_DEG);
            assert(target >= Config::SERVO_MIN_DEG && target <= Config::SERVO_MAX_DEG && isfinite(target));
            const float current = Config::SERVO_HOME_DEG, maxStep = Config::SERVO_SPEED_DPS * .02f;
            auto next = Lamp::approach(current, target, maxStep);
            assert(fabsf(next - current) <= maxStep + .00001f);
            assert(fabsf(next - target) <= fabsf(current - target) + .00001f);
            for (unsigned strip = 0; strip < 4; ++strip) {
                for (unsigned p = 0; p < 30; ++p) {
                    auto c = Lamp::color(scene.effect, time, strip, 0, p, 30);
                    if (scene.effect == Lamp::Effect::Off) assert(c.r + c.g + c.b == 0);
                    if (scene.effect == Lamp::Effect::Night && strip != 1) assert(c.r + c.g + c.b == 0);
                }
            }
        }
    }
    auto front = Lamp::color(Lamp::Effect::Reading, 0, 2, 0, 0, 30);
    auto top = Lamp::color(Lamp::Effect::Reading, 0, 0, 0, 0, 30);
    assert(front.r > top.r && front.g > top.g && front.b > top.b);
    const float minPulse = Config::SERVO_MIN_US + Config::SERVO_MIN_DEG / Config::SERVO_RANGE_DEG *
                           (Config::SERVO_MAX_US - Config::SERVO_MIN_US);
    const float maxPulse = Config::SERVO_MIN_US + Config::SERVO_MAX_DEG / Config::SERVO_RANGE_DEG *
                           (Config::SERVO_MAX_US - Config::SERVO_MIN_US);
    assert(fabsf(minPulse - 1400) < .01f && fabsf(maxPulse - 1600) < .01f);
    assert(Lamp::due(15, UINT32_MAX - 10, 25));
    assert(!Lamp::due(15, UINT32_MAX - 10, 27));
}
void testPower() {
    for (uint32_t sum = 0; sum <= 120 * 3 * 255; sum += 101) {
        for (unsigned level = 0; level <= 255; ++level) {
            uint8_t applied = Lamp::limitedBrightness(sum, level, 4000);
            assert(applied <= level);
            assert(uint64_t(sum) * 20 * applied <= uint64_t(4000) * 255 * 255);
        }
    }
    assert(Lamp::limitedBrightness(0, 255, 0) == 255); // black frame draws no modelled RGB current
    assert(Lamp::limitedBrightness(120 * 3 * 255, 255, 4000) == 141);
}
JsonDocument validPacket() {
    JsonDocument d;
    d["v"] = Lamp::PROTOCOL_VERSION;
    d["type"] = "set";
    d["key"] = LAMP_PAIRING_KEY;
    d["boot"] = 123;
    d["session"] = 456;
    d["seq"] = 1;
    d["scene"] = 4;
    d["brightness"] = 255;
    d["motion"] = true;
    return d;
}
void testProtocol() {
    Lamp::Command cmd{};
    auto d = validPacket();
    assert(Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    assert(cmd.scene == 4 && cmd.brightness == 255 && cmd.motion && cmd.session == 456);
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 124, cmd)); // restarted lamp
    for (const char* missing : {"v", "type", "key", "boot", "session", "seq", "scene", "brightness", "motion"}) {
        auto invalid = validPacket();
        invalid.remove(missing);
        assert(!Lamp::decodeCommand(invalid.as<JsonObjectConst>(), 123, cmd));
    }
    for (const char* bad : {"-1", "256", "1.5", "true", "\"60\"", "null", "{}", "[]"}) {
        auto invalid = validPacket();
        JsonDocument value;
        assert(!deserializeJson(value, bad));
        invalid["brightness"].set(value.as<JsonVariantConst>());
        assert(!Lamp::decodeCommand(invalid.as<JsonObjectConst>(), 123, cmd));
    }
    d = validPacket(); d["scene"] = Lamp::SCENE_COUNT;
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["motion"] = 1;
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["key"] = "wrong";
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["session"] = 0;
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["v"] = "1";
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    Lamp::CommandGate gate;
    using Order = Lamp::CommandOrder;
    assert(gate.inspect(100, UINT32_MAX - 1) == Order::New);
    assert(gate.inspect(100, UINT32_MAX - 1) == Order::Duplicate);
    assert(gate.inspect(100, UINT32_MAX - 2) == Order::Stale);
    assert(gate.inspect(100, 0) == Order::New); // sequence rollover
    assert(gate.inspect(100, UINT32_MAX) == Order::Stale);
    assert(gate.inspect(101, 1) == Order::New); // new controller boot
}
int main() {
    testMapping(); testMotionAndEffects(); testPower(); testProtocol();
    std::cout << "PASS: LED mapping, scene effects, motion limits, current estimate, packet validation, retries/sequence rollover\n";
}
