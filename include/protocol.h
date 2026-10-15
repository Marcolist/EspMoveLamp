#pragma once
#include <ArduinoJson.h>
#include "config.h"
#include "scenes.h"

namespace Lamp {
struct ManualSettings {
    float target = Config::SERVO_HOME_DEG;
    uint32_t colors[3] = {0xff943f, 0xff943f, 0xffdcaa};
};
inline bool decodeManual(JsonObjectConst data, bool& manual, ManualSettings& out) {
    if (!data["manual"].is<bool>() || !data["target"].is<float>()) return false;
    float target = data["target"];
    if (!isfinite(target) || target < Config::SERVO_MIN_DEG || target > Config::SERVO_MAX_DEG)
        return false;
    JsonArrayConst colors = data["colors"].as<JsonArrayConst>();
    if (colors.size() != 3) return false;
    ManualSettings candidate;
    candidate.target = target;
    for (unsigned i = 0; i < 3; ++i) {
        if (!colors[i].is<uint32_t>() || colors[i].as<uint32_t>() > 0xffffff) return false;
        candidate.colors[i] = colors[i];
    }
    manual = data["manual"];
    out = candidate;
    return true;
}
inline void encodeManual(JsonDocument& data, bool manual, const ManualSettings& settings) {
    data["manual"] = manual;
    data["target"] = settings.target;
    JsonArray colors = data["colors"].to<JsonArray>();
    for (auto color : settings.colors) colors.add(color);
}
inline RGB manualColor(const ManualSettings& settings, uint8_t section) {
    uint32_t color = settings.colors[section < 2 ? section : 2];
    return {uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color)};
}
struct Command {
    uint32_t session, seq;
    uint8_t scene, brightness;
    bool motion;
    bool manual = false;
    ManualSettings settings;
};
inline bool validEnvelope(JsonObjectConst data) {
    return data["v"].is<uint8_t>() && data["v"] == PROTOCOL_VERSION &&
           data["key"].is<const char*>() && data["key"] == LAMP_PAIRING_KEY;
}
inline bool decodeCommand(JsonObjectConst data, uint32_t boot, Command& out) {
    if (!validEnvelope(data) || data["type"] != "set" ||
        !data["boot"].is<uint32_t>() || data["boot"] != boot ||
        !data["session"].is<uint32_t>() || !data["seq"].is<uint32_t>() ||
        !data["scene"].is<uint8_t>() || !data["brightness"].is<uint8_t>() ||
        !data["motion"].is<bool>() || data["scene"].as<unsigned>() >= SCENE_COUNT ||
        data["session"] == 0) return false;
    bool manual;
    ManualSettings settings;
    if (!decodeManual(data, manual, settings)) return false;
    out.session = data["session"];
    out.seq = data["seq"];
    out.scene = data["scene"];
    out.brightness = data["brightness"];
    out.motion = data["motion"];
    out.manual = manual;
    out.settings = settings;
    return true;
}
enum class CommandOrder { New, Duplicate, Stale };
class CommandGate {
public:
    CommandOrder inspect(uint32_t session, uint32_t seq) {
        if (have_ && session == session_) {
            if (seq == seq_) return CommandOrder::Duplicate;
            if (!newer(seq, seq_)) return CommandOrder::Stale;
        }
        have_ = true;
        session_ = session;
        seq_ = seq;
        return CommandOrder::New;
    }
private:
    bool have_ = false;
    uint32_t session_ = 0, seq_ = 0;
};
}
