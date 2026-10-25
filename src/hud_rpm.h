#ifndef HUD_RPM_H
#define HUD_RPM_H

#include <stdint.h>

static inline uint16_t hud_rpm_needle_rotation(float normalizedRpm) {
    if (!(normalizedRpm >= 0.0f)) normalizedRpm = 0.0f;
    if (normalizedRpm > 1.0f) normalizedRpm = 1.0f;
    const float startUnits = 310.8f * (65536.0f / 360.0f);
    const float sweepUnits = 100.0f * (65536.0f / 360.0f);
    // The sweep crosses 65536. Integer narrowing wraps; float narrowing does not.
    return (uint16_t) (uint32_t) (startUnits + normalizedRpm * sweepUnits);
}

#endif
