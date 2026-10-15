#pragma once
#include <ArduinoJson.h>
#include "config.h"
#include "scenes.h"

namespace Lamp {
struct Command {
    uint32_t session, seq;
    uint8_t scene, brightness;
    bool motion;
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
    out = {data["session"].as<uint32_t>(), data["seq"].as<uint32_t>(),
           data["scene"].as<uint8_t>(), data["brightness"].as<uint8_t>(),
           data["motion"].as<bool>()};
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
