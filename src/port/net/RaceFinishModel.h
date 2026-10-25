#pragma once

#include <cmath>

namespace KartNet {

constexpr bool FinishEligible(int connectedHumans, bool unfinished, bool timedOut) {
    return connectedHumans > 0 && (!unfinished || timedOut);
}

class FinishHold {
  public:
    static constexpr float HoldSeconds = 5.f;

    void Reset() {
        holding_ = false;
        startedAt_ = lastTime_ = 0.0;
    }

    bool Update(bool eligible, float courseTime) {
        if (!eligible || !std::isfinite(courseTime)) {
            Reset();
            return false;
        }

        const double now = static_cast<double>(courseTime);
        // Eligibility starts on this frame, including after a clock regression.
        if (!holding_ || now < lastTime_) {
            holding_ = true;
            startedAt_ = now;
        }
        lastTime_ = now;
        return now - startedAt_ >= static_cast<double>(HoldSeconds);
    }

  private:
    bool holding_ = false;
    double startedAt_ = 0.0;
    double lastTime_ = 0.0;
};

} // namespace KartNet
