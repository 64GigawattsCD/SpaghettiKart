#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace KartAudio {
inline int16_t MixSample(int16_t native, float custom, float volume) {
    // Scale before narrowing; float-to-int conversion must stay inside S16 range.
    const float mixed = native + custom * 32767.0f * volume;
    if (std::isnan(mixed)) return native;
    return static_cast<int16_t>(std::clamp(mixed, -32768.0f, 32767.0f));
}

inline bool NeedsRefill(int queued, int target) { return queued < target; }

inline uint32_t AndroidSamplesPerTick(uint64_t block) {
    // Two synthesis ticks per block: (11 * 448 + 432) * 2 / 12 * 30 = 26800 Hz.
    return block % 12 == 11 ? 432 : 448;
}
}
