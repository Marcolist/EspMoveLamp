#pragma once
#include <string.h>
#include "protocol.h"

namespace Lamp {
constexpr uint8_t MAX_SAVED_SCENES = 32;
struct SavedScene {
    uint32_t id = 0;
    char name[33] = {};
    uint32_t tint = 0;
    uint8_t icon = 0, preset = 0, brightness = Config::INITIAL_BRIGHTNESS;
    bool motion = false;
    ManualSettings settings;
};
struct SceneLibrary {
    uint32_t revision = 1;
    uint8_t count = 0;
    SavedScene scenes[MAX_SAVED_SCENES];
    uint32_t favorites[4] = {};
};
inline const SavedScene* findScene(const SceneLibrary& library, uint32_t id) {
    for (uint8_t i = 0; i < library.count; ++i) if (library.scenes[i].id == id) return &library.scenes[i];
    return nullptr;
}
inline bool validLibrary(const SceneLibrary& library) {
    if (library.count < 4 || library.count > MAX_SAVED_SCENES || !library.revision || library.revision == UINT32_MAX) return false;
    for (uint8_t i = 0; i < library.count; ++i) {
        const auto& s = library.scenes[i];
        const char* end = static_cast<const char*>(memchr(s.name, 0, sizeof(s.name)));
        if (!end || end == s.name || s.id == 0 || s.id > INT32_MAX || s.tint > 0xffffff || s.icon > 7 || s.preset >= SCENE_COUNT ||
            !isfinite(s.settings.target) || s.settings.target < Config::SERVO_MIN_DEG || s.settings.target > Config::SERVO_MAX_DEG) return false;
        for (const char* p = s.name; p < end; ++p) if (static_cast<unsigned char>(*p) < 32) return false;
        for (auto color : s.settings.colors) if (color > 0xffffff) return false;
        for (uint8_t j = 0; j < i; ++j) if (library.scenes[j].id == s.id) return false;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        if (!findScene(library, library.favorites[i])) return false;
        for (uint8_t j = 0; j < i; ++j) if (library.favorites[j] == library.favorites[i]) return false;
    }
    return true;
}
inline SceneLibrary defaultLibrary() {
    SceneLibrary library;
    const char* names[] = {"Meeting", "Abend", "Fokus", "Show"};
    const uint8_t presets[] = {0, 0, 0, 5};
    const uint32_t tints[] = {0x47c8f1, 0xf35a9b, 0x4fe0b5, 0x9871f5};
    library.count = 4;
    for (uint8_t i = 0; i < 4; ++i) {
        auto& s = library.scenes[i];
        s.id = i + 1; strcpy(s.name, names[i]); s.icon = i + 4; s.preset = presets[i]; s.tint = tints[i];
        s.settings.colors[0] = 0; s.settings.colors[1] = 0; s.settings.colors[2] = 0;
        s.brightness = i == 0 ? 190 : i == 3 ? 100 : 90;
        if (i == 0) s.settings.colors[2] = 0xfff3e4;
        if (i == 1) s.settings.colors[0] = 0xff0818;
        if (i == 2) { s.settings.colors[0] = 0xd2c4ff; s.settings.target = Config::SERVO_MAX_DEG; }
        s.motion = i >= 2;
        library.favorites[i] = s.id;
    }
    return library;
}
inline void encodeLibrary(JsonDocument& doc, const SceneLibrary& library) {
    doc["schema"] = 1; doc["revision"] = library.revision;
    JsonArray scenes = doc["scenes"].to<JsonArray>();
    for (uint8_t i = 0; i < library.count; ++i) {
        const auto& s = library.scenes[i];
        JsonObject out = scenes.add<JsonObject>();
        out["id"] = s.id; out["name"] = s.name; out["icon"] = s.icon; out["tint"] = s.tint;
        out["preset"] = s.preset; out["brightness"] = s.brightness; out["motion"] = s.motion;
        out["manual"] = s.preset == 0; out["target"] = s.settings.target;
        JsonArray colors = out["colors"].to<JsonArray>();
        for (auto color : s.settings.colors) colors.add(color);
    }
    JsonArray favorites = doc["favorites"].to<JsonArray>();
    for (auto id : library.favorites) favorites.add(id);
}
inline bool decodeLibrary(JsonObjectConst doc, SceneLibrary& out) {
    if (!doc["schema"].is<uint8_t>() || doc["schema"] != 1 || !doc["revision"].is<uint32_t>()) return false;
    JsonArrayConst scenes = doc["scenes"].as<JsonArrayConst>();
    JsonArrayConst favorites = doc["favorites"].as<JsonArrayConst>();
    if (scenes.size() < 4 || scenes.size() > MAX_SAVED_SCENES || favorites.size() != 4) return false;
    SceneLibrary candidate;
    candidate.revision = doc["revision"]; candidate.count = scenes.size();
    for (uint8_t i = 0; i < candidate.count; ++i) {
        JsonObjectConst data = scenes[i]; auto& s = candidate.scenes[i];
        if (!data["id"].is<uint32_t>() || !data["name"].is<const char*>() || !data["icon"].is<uint8_t>() ||
            !data["tint"].is<uint32_t>() || !data["preset"].is<uint8_t>() || !data["brightness"].is<uint8_t>() || !data["motion"].is<bool>()) return false;
        const char* name = data["name"];
        size_t length = strlen(name);
        if (length == 0 || length > 32 || data["name"].as<JsonString>().size() != length) return false;
        strcpy(s.name, name); s.id = data["id"]; s.icon = data["icon"]; s.tint = data["tint"];
        s.preset = data["preset"]; s.brightness = data["brightness"]; s.motion = data["motion"];
        bool manual;
        if (!decodeManual(data, manual, s.settings) || manual != (s.preset == 0)) return false;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        if (!favorites[i].is<uint32_t>()) return false;
        candidate.favorites[i] = favorites[i];
    }
    if (!validLibrary(candidate)) return false;
    out = candidate;
    return true;
}
inline bool sceneMatches(const SavedScene& s, uint8_t scene, uint8_t brightness, bool motion,
                         bool manual, const ManualSettings& settings) {
    if (scene != (s.preset ? s.preset : 1) || brightness != s.brightness || motion != s.motion || manual != (s.preset == 0)) return false;
    if (!manual) return true;
    if (fabsf(settings.target - s.settings.target) > .01f) return false;
    for (uint8_t i = 0; i < 3; ++i) if (settings.colors[i] != s.settings.colors[i]) return false;
    return true;
}
}
