#pragma once
#include "config.h"
#include "scenes.h"
namespace Lamp {
class ReferenceReturn {
    bool active_ = false, settling_ = false;
    uint32_t reachedAt_ = 0;
public:
    void start() { active_ = true; settling_ = false; }
    void cancel() { active_ = false; settling_ = false; }
    bool active() const { return active_; }
    void tick(uint32_t now, float commandedAngle) {
        if (!active_) return;
        if (fabsf(commandedAngle - Config::SERVO_REFERENCE_DEG) >= .01f) {
            settling_ = false; return;
        }
        if (!settling_) { reachedAt_ = now; settling_ = true; }
        if (due(now,reachedAt_,700)) cancel();
    }
};
}
