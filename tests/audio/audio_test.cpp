#include "../../src/port/audio/AudioMix.h"
#include <cstdlib>
#include <iostream>
#include <limits>

void check(bool pass, const char* name) {
    if (!pass) { std::cerr << name << '\n'; std::exit(1); }
}

int main() {
    using namespace KartAudio;
    check(MixSample(20000, 1, 1) == 32767, "positive saturation");
    check(MixSample(-20000, -1, 1) == -32768, "negative saturation");
    check(MixSample(0, 2, .25f) == 16383, "volume before conversion");
    check(MixSample(1234, 2, 0) == 1234, "native bus not scaled twice");
    check(MixSample(0, .5f, 1) == 16383, "in-range signal");
    check(MixSample(1234, std::numeric_limits<float>::quiet_NaN(), 1) == 1234, "NaN");
    check(MixSample(0, std::numeric_limits<float>::infinity(), 1) == 32767, "positive infinity");
    check(MixSample(0, -std::numeric_limits<float>::infinity(), 1) == -32768, "negative infinity");
    check(!NeedsRefill(1100, 1100) && !NeedsRefill(6000, 1100), "bounded queue");
    check(NeedsRefill(0, 1100) && NeedsRefill(1099, 1100), "refill threshold");
    uint64_t frames = 0;
    for (uint64_t block = 0; block < 360; ++block) frames += AndroidSamplesPerTick(block) * 2;
    check(frames == 26800 * 12, "native sample rate over twelve seconds");

    // Millisecond model: renderer stalls, then the device stops consuming for 1 s.
    double queued = 0, oldQueued = 0;
    uint64_t block = 0;
    int empty = 0, oldEmpty = 0;
    for (int ms = 0; ms < 15000; ++ms) {
        if (ms < 5000 || ms >= 6000) {
            queued = std::max(0.0, queued - 26.8);
            oldQueued = std::max(0.0, oldQueued - 26.8);
            if (ms > 100) { empty += queued == 0; oldEmpty += oldQueued == 0; }
        }
        if (ms % 8 == 0 && NeedsRefill(int(queued), 1100)) queued += AndroidSamplesPerTick(block++) * 2;
        if (ms % 40 == 0 && oldQueued < 6000) oldQueued += 896;
        check(queued < 2000, "device pause does not accumulate unbounded audio");
    }
    check(empty == 0 && oldEmpty > 0, "audio survives slow render and device resume");
    std::cout << "Audio mix and queue checks passed\n";
}
