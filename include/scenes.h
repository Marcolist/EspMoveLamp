#pragma once
#include <math.h>
#include <stdint.h>

namespace Lamp {
enum class Effect : uint8_t { Off, Warm, Reading, Sunset, Ocean, Rainbow, Candle, Night };
struct Scene {
    const char* name;
    const char* detail;
    Effect effect;
    float position; // 0..1 within configured mechanical limits
    float swing;    // amplitude within configured range
    uint32_t periodMs;
    uint16_t accent; // RGB565 for display
};
constexpr Scene SCENES[] = {
    {"Aus",        "Licht aus / Servo haelt", Effect::Off,     .50f, 0,    0,     0x630c},
    {"Warm",       "Ruhiges warmes Licht",    Effect::Warm,    .50f, 0,    0,     0xfd28},
    {"Lesen",      "Helles Licht nach vorn",  Effect::Reading, .25f, 0,    0,     0xffff},
    {"Sonnenbad",  "Orange / sanfte Bewegung",Effect::Sunset,  .60f, .15f, 24000, 0xfb20},
    {"Ozean",      "Blaue Wellen / Schwenken",Effect::Ocean,   .50f, .30f, 18000, 0x067f},
    {"Regenbogen", "Farben / langsamer Lauf", Effect::Rainbow, .50f, .40f, 30000, 0xc81f},
    {"Kerze",      "Warmes Flackern",         Effect::Candle,  .55f, 0,    0,     0xfc00},
    {"Nacht",      "Sehr dezentes Rot",       Effect::Night,   .80f, 0,    0,     0x8804},
};
constexpr uint8_t SCENE_COUNT = sizeof(SCENES) / sizeof(SCENES[0]);
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr const char* DEVICE_ID = "espmovelamp-1";

inline float clamp(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
inline float approach(float current, float target, float maxStep) {
    return current + clamp(target - current, -maxStep, maxStep);
}
inline float targetAngle(const Scene& scene, uint32_t elapsed, float lo, float hi) {
    float pos = scene.position;
    if (scene.periodMs) {
        pos += scene.swing * sinf(float(elapsed % scene.periodMs) / scene.periodMs * 6.2831853f);
    }
    return lo + clamp(pos, 0, 1) * (hi - lo);
}
inline uint16_t pixelIndex(uint8_t section, uint8_t row, uint16_t pixel,
                           uint8_t rows, uint16_t perRow, bool serpentine) {
    return (section * rows + row) * perRow +
           (serpentine && (row & 1) ? perRow - 1 - pixel : pixel);
}
// Works across millis() wrap; all intervals must be less than 2^31 ms.
inline bool due(uint32_t now, uint32_t then, uint32_t interval) {
    return uint32_t(now - then) >= interval;
}
inline bool newer(uint32_t seq, uint32_t previous) {
    return int32_t(seq - previous) > 0;
}
inline uint8_t limitedBrightness(uint32_t channelSum, uint8_t requested, uint32_t limitMa) {
    const uint64_t current = uint64_t(channelSum) * 20 * requested;
    const uint64_t budget = uint64_t(limitMa) * 255 * 255;
    return current > budget ? uint64_t(requested) * budget / current : requested;
}
struct RGB { uint8_t r, g, b; };
inline RGB hsv(uint8_t h, uint8_t s, uint8_t v) {
    const uint8_t region = h / 43;
    const uint8_t f = (h - region * 43) * 6;
    const uint8_t p = uint16_t(v) * (255 - s) / 255;
    const uint8_t q = uint16_t(v) * (255 - uint16_t(s) * f / 255) / 255;
    const uint8_t t = uint16_t(v) * (255 - uint16_t(s) * (255 - f) / 255) / 255;
    switch (region) {
        case 0: return {v,t,p}; case 1: return {q,v,p}; case 2: return {p,v,t};
        case 3: return {p,q,v}; case 4: return {t,p,v}; default: return {v,p,q};
    }
}
inline RGB color(Effect effect, uint32_t ms, uint8_t section, uint8_t row,
                 uint16_t pixel, uint16_t perRow) {
    float x = perRow > 1 ? float(pixel) / (perRow - 1) : 0;
    const float wave = .5f + .5f * sinf(x * 6.2831853f + float(ms % 6000) / 955.0f + section + row);
    switch (effect) {
        case Effect::Warm: return section < 2 ? RGB{160, 80, 30} : RGB{255, 148, 65};
        case Effect::Reading: return section >= 2 ? RGB{255, 220, 170} : RGB{65, 45, 25};
        case Effect::Sunset: return {255, uint8_t(45 + wave * 75), uint8_t(4 + x * 12)};
        case Effect::Ocean: return {0, uint8_t(35 + wave * 90), uint8_t(100 + wave * 155)};
        case Effect::Rainbow: return hsv(uint8_t(ms / 35 + pixel * 255 / perRow + section * 35 + row * 55), 240, 255);
        case Effect::Candle: {
            // Deterministic, spatially correlated flicker; no frame-dependent randomness.
            uint32_t n = (ms / 75) + section * 193 + row * 37;
            n = (n ^ (n >> 16)) * 0x45d9f3b;
            uint8_t v = 155 + n % 101;
            return {v, uint8_t(v * .35f), uint8_t(v * .025f)};
        }
        case Effect::Night: return section == 1 ? RGB{18, 1, 0} : RGB{0, 0, 0};
        default: return {0, 0, 0};
    }
}
}
