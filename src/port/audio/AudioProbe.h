#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace KartAudio {
inline bool ProbeEnabled() {
    static const bool enabled = std::getenv("SK_TEST_ROOT") && std::getenv("SK_AUDIO_PROBE");
    return enabled;
}

// Owned entirely by the audio thread. PCM is recorded before output is muted.
class Probe {
    using Clock = std::chrono::steady_clock;
    Clock::time_point previous{};
    uint64_t ticks = 0, measured = 0, empty = 0, clipped = 0, nonzero = 0, bytes = 0;
    int minimum = INT32_MAX, maximum = 0;
    double maxGapMs = 0, sumGapMs = 0, maxMixMs = 0;
    std::ofstream wave;

    void word(uint32_t n, int width) {
        for (int i = 0; i < width; ++i) wave.put(char((n >> (8 * i)) & 255));
    }
    void header(uint32_t rate) {
        wave.seekp(0);
        wave.write("RIFF", 4); word(36 + bytes, 4); wave.write("WAVEfmt ", 8);
        word(16, 4); word(1, 2); word(2, 2); word(rate, 4); word(rate * 4, 4);
        word(4, 2); word(16, 2); wave.write("data", 4); word(bytes, 4);
        wave.seekp(0, std::ios::end);
    }

  public:
    void record(int queued, const int16_t* pcm, size_t count, uint64_t overrange,
                uint32_t rate, const char* driver, Clock::time_point started) {
        if (!ProbeEnabled()) return;
        const auto now = Clock::now();
        const double gap = previous == Clock::time_point{} ? 0 :
            std::chrono::duration<double, std::milli>(started - previous).count();
        previous = started;
        if (++ticks <= 60) return;
        ++measured;
        empty += queued == 0;
        clipped += overrange;
        minimum = std::min(minimum, queued);
        maximum = std::max(maximum, queued);
        maxGapMs = std::max(maxGapMs, gap);
        sumGapMs += gap;
        maxMixMs = std::max(maxMixMs, std::chrono::duration<double, std::milli>(now - started).count());
        const auto root = std::filesystem::path(std::getenv("SK_TEST_ROOT"));
        if (measured <= 600) {
            if (!wave.is_open()) {
                wave.open(root / "audio.wav", std::ios::binary | std::ios::trunc);
                header(rate);
            }
            for (size_t i = 0; i < count; ++i) nonzero += pcm[i] != 0;
            wave.write(reinterpret_cast<const char*>(pcm), count * sizeof(int16_t));
            bytes += count * sizeof(int16_t);
        }
        if (measured % 30 != 0) return;
        header(rate);
        wave.flush();
        nlohmann::json report = {{"ticks", ticks}, {"measured_ticks", measured}, {"empty_queue_ticks", empty},
            {"queue_min_frames", minimum}, {"queue_max_frames", maximum}, {"max_gap_ms", maxGapMs},
            {"mean_gap_ms", sumGapMs / measured}, {"max_mix_ms", maxMixMs}, {"overrange_samples", clipped},
            {"nonzero_samples", nonzero}, {"pcm_bytes", bytes}, {"sample_rate", rate},
            {"driver", driver ? driver : "none"}, {"output_muted", true}};
        const auto temp = root / "audio-report.tmp";
        { std::ofstream out(temp); out << report.dump(2); }
        std::error_code ec;
        std::filesystem::rename(temp, root / "audio-report.json", ec);
        if (ec) {
            std::filesystem::remove(root / "audio-report.json", ec);
            std::filesystem::rename(temp, root / "audio-report.json", ec);
        }
    }
};
}
