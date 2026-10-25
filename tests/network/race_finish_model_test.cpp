#include "../../src/port/net/RaceFinishModel.h"

#include <cstdlib>
#include <iostream>
#include <limits>

static int checks = 0;
static void check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::cerr << "Race-finish check failed: " << message << '\n';
        std::exit(1);
    }
}

int main() {
    using KartNet::FinishEligible;
    using KartNet::FinishHold;
    static_assert(FinishHold::HoldSeconds == 5.f);
    static_assert(!FinishEligible(0, false, true));
    static_assert(FinishEligible(1, false, false));

    FinishHold hold;
    check(!hold.Update(false, 120.f), "unfinished race");
    check(!hold.Update(true, 123.25f), "actual crossing frame starts hold");
    check(!hold.Update(true, std::nextafter(128.25f, 0.f)), "just below five seconds");
    check(hold.Update(true, 128.25f), "exactly five seconds");
    check(hold.Update(true, std::nextafter(128.25f, 200.f)), "just above five seconds");
    check(hold.Update(true, 200.f), "completed hold stays complete");
    check(hold.Update(true, 200.f), "completed paused hold stays complete");

    check(!hold.Update(false, 200.f), "ineligibility cancels completed hold");
    check(!hold.Update(true, 201.f), "new eligibility starts fresh");
    check(!hold.Update(true, 205.f), "four seconds");
    check(!hold.Update(false, 205.f), "ineligibility cancels pending hold");
    check(!hold.Update(true, 210.f), "time while ineligible is not credited");
    check(!hold.Update(true, 214.99f), "fresh hold still pending");
    check(hold.Update(true, 215.f), "fresh hold completes");

    hold.Reset();
    check(!hold.Update(true, 215.f), "explicit reset clears completed hold");
    check(!hold.Update(true, 216.f), "new hold pending");
    hold.Reset();
    check(!hold.Update(true, 0.f), "new race starts at zero");
    check(!hold.Update(true, 4.f), "previous race contributes no time");
    check(hold.Update(true, 5.f), "new race completes its own hold");

    hold.Reset();
    check(!hold.Update(true, 50.f), "pause test starts");
    for (int i = 0; i < 1000; ++i) {
        check(!hold.Update(true, 50.f), "paused frames contribute no time");
    }
    check(!hold.Update(true, 54.999f), "pause did not advance hold");
    check(hold.Update(true, 55.f), "course clock resumes");

    hold.Reset();
    check(!hold.Update(true, 10.f), "variable frames start");
    for (float time : {10.016f, 10.051f, 10.7f, 12.f, 12.001f, 14.9f, 14.999f}) {
        check(!hold.Update(true, time), "variable frame increments below threshold");
    }
    check(hold.Update(true, 15.125f), "first frame beyond threshold");

    hold.Reset();
    check(!hold.Update(true, std::nextafter(1.f, 2.f)), "fractional crossing time");
    check(!hold.Update(true, 6.f), "double subtraction does not round early to five");
    check(hold.Update(true, std::nextafter(6.f, 7.f)), "fractional crossing completes");

    hold.Reset();
    check(!hold.Update(true, 100.f), "regression test starts");
    check(!hold.Update(true, 104.f), "regression test pending");
    check(!hold.Update(true, 103.f), "backward clock restarts hold");
    check(!hold.Update(true, 107.999f), "old start time discarded");
    check(hold.Update(true, 108.f), "five seconds after regression");
    check(!hold.Update(true, 1.f), "backward clock clears completed hold");
    check(!hold.Update(true, 5.999f), "completed regression still pending");
    check(hold.Update(true, 6.f), "completed regression gets full new hold");

    for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity()}) {
        hold.Reset();
        check(!hold.Update(true, invalid), "non-finite initial clock");
        check(!hold.Update(true, 10.f), "finite clock starts after invalid");
        check(!hold.Update(true, 14.f), "pending before invalid clock");
        check(!hold.Update(true, invalid), "non-finite clock cancels pending hold");
        check(!hold.Update(true, 1000.f), "invalid clock does not fast-forward finish");
        check(!hold.Update(true, 1004.999f), "full hold required after invalid clock");
        check(hold.Update(true, 1005.f), "finite recovery completes");
        check(!hold.Update(true, invalid), "non-finite clock clears completed hold");
        check(!hold.Update(true, 2000.f), "completed hold restarts after invalid clock");
        check(!hold.Update(false, invalid), "ineligible invalid clock remains pending");
    }

    for (int humans : {-1, 0}) {
        for (bool unfinished : {false, true}) {
            for (bool timedOut : {false, true}) {
                check(!FinishEligible(humans, unfinished, timedOut), "no connected humans");
            }
        }
    }
    for (int humans = 1; humans <= 8; ++humans) {
        check(!FinishEligible(humans, true, false), "unfinished humans delay eligibility");
        check(FinishEligible(humans, false, false), "last human finished");
        check(FinishEligible(humans, true, true), "DNF timeout permits eligibility");
        check(FinishEligible(humans, false, true), "finished with timeout");
        for (bool timedOut : {false, true}) {
            hold.Reset();
            check(!hold.Update(FinishEligible(humans, true, false), 59.f), "roster still racing");
            check(!hold.Update(FinishEligible(humans, timedOut, timedOut), 60.f),
                  "solo or multi eligibility starts hold, including DNF");
            check(!hold.Update(FinishEligible(humans, timedOut, timedOut), 64.999f),
                  "solo or multi must linger full five seconds");
            check(hold.Update(FinishEligible(humans, false, timedOut), 65.f),
                  "solo or multi hold completes even if eligibility reason changes");
        }
    }

    std::cout << checks << " race-finish checks passed\n";
}
