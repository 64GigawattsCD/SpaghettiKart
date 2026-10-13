#pragma once

#include <cstdint>

namespace FrameInterpolation {

// Binary angles wrap at 65536; large discontinuities retain the existing snap behavior.
inline int16_t InterpolateBinaryAngle(int16_t oldAngle, int16_t newAngle, float step) {
    if (oldAngle == newAngle) {
        return newAngle;
    }
    int32_t oldUnsigned = static_cast<uint16_t>(oldAngle);
    int32_t newUnsigned = static_cast<uint16_t>(newAngle);
    int32_t delta = oldUnsigned - newUnsigned;
    if (delta < -0x8000 || delta > 0x8000) {
        if (oldUnsigned < newUnsigned) {
            oldUnsigned += 0x10000;
        } else {
            newUnsigned += 0x10000;
        }
        delta = oldUnsigned - newUnsigned;
    }
    if (delta < -0x4000 || delta > 0x4000) {
        return newAngle;
    }
    // Narrow through an integer so interpolation across 65536 wraps deterministically.
    const auto blended = static_cast<int32_t>((1.0f - step) * oldUnsigned + step * newUnsigned);
    return static_cast<int16_t>(static_cast<uint16_t>(blended));
}

} // namespace FrameInterpolation
