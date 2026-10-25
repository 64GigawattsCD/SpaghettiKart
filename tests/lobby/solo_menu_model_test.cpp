#include "../../src/port/ui/SoloMenuModel.h"
#include "../../src/port/ui/DriverSelectModel.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace GameUI::SoloMenu;

static int checks = 0;
static void Require(bool condition, int line) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "Solo menu check %d failed at line %d\n", checks, line);
        std::abort();
    }
}
#define CHECK(condition) Require((condition), __LINE__)

static_assert(Enabled(true, true, 1));
static_assert(!Enabled(false, true, 1));
static_assert(!Enabled(true, false, 1));
static_assert(!ShowCapacity(true) && ShowCapacity(false));
static_assert(RowsPerPage == 6);
static_assert(ClampSelection(8, 7) == 7);
static_assert(MoveSelection(6, 1, 7) == 7);
static_assert(PageFirst(6, 6) == 0);
static_assert(PageFirst(7, 7) == 6);

static void CheckEligibility() {
    for (bool explicitSolo : {false, true}) {
        CHECK(ShowCapacity(explicitSolo) == !explicitSolo);
        for (bool localOnly : {false, true}) {
            for (int count = -16; count <= 16; ++count)
                CHECK(Enabled(explicitSolo, localOnly, count) == (explicitSolo && localOnly && count == 1));
            CHECK(!Enabled(explicitSolo, localOnly, std::numeric_limits<int>::min()));
            CHECK(!Enabled(explicitSolo, localOnly, std::numeric_limits<int>::max()));
        }
    }
    // Each one-bit mask is solo-capable, including a primary player on a nonzero port.
    for (unsigned mask = 0; mask < 16; ++mask) {
        int count = 0;
        for (int port = 0; port < 4; ++port) if (mask & (1u << port)) ++count;
        const bool onePort = mask == 1 || mask == 2 || mask == 4 || mask == 8;
        CHECK(Enabled(true, true, count) == onePort);
        CHECK(!Enabled(false, true, count));
        CHECK(!Enabled(true, false, count));
    }
}

static int ExpectedSelection(int selected, int count) {
    if (count < 0 || selected < 0) return 0;
    return selected <= count ? selected : count;
}

static int ExpectedPage(int selected, int count) {
    if (count <= 0) return 0;
    int page = 0;
    // A footer at an exact page multiple remains on the preceding settings page.
    while (page + 6 < count && page + 6 <= selected) page += 6;
    return page;
}

static void CheckNavigation() {
    const std::array<int, 7> directions{std::numeric_limits<int>::min(), -2, -1, 0, 1, 2,
                                         std::numeric_limits<int>::max()};
    for (int count = -2; count <= 72; ++count) {
        for (int selected = -3; selected <= 75; ++selected) {
            const int current = ExpectedSelection(selected, count);
            CHECK(ClampSelection(selected, count) == current);
            CHECK(PageFirst(selected, count) == ExpectedPage(current, count));
            const int first = PageFirst(selected, count);
            CHECK(first >= 0 && first % 6 == 0);
            if (count > 0) {
                CHECK(first < count);
                CHECK(current == count || (current >= first && current < first + 6));
            }
            for (int direction : directions) {
                int expected = current;
                if (direction < 0 && expected != 0) --expected;
                if (direction > 0 && expected < count) ++expected;
                CHECK(MoveSelection(selected, direction, count) == expected);
            }
        }
        if (count < 0) continue;
        int selected = 0;
        for (int row = 0; row < count; ++row) {
            CHECK(selected == row);
            selected = MoveSelection(selected, 1, count);
        }
        CHECK(selected == count);
        CHECK(MoveSelection(selected, 1, count) == count);
        CHECK(PageFirst(selected, count) == PageFirst(count > 0 ? count - 1 : 0, count));
        for (int row = count; row > 0; --row) {
            CHECK(selected == row);
            selected = MoveSelection(selected, -1, count);
        }
        CHECK(selected == 0);
    }
    // Shrinking option lists retains NEXT, not an out-of-range selection.
    CHECK(ClampSelection(8, 7) == 7);
    CHECK(ClampSelection(7, 5) == 5);
    const int min = std::numeric_limits<int>::min(), max = std::numeric_limits<int>::max();
    CHECK(ClampSelection(min, max) == 0);
    CHECK(ClampSelection(max, min) == 0);
    CHECK(ClampSelection(max, max) == max);
    CHECK(MoveSelection(max, 1, max) == max);
    CHECK(MoveSelection(max, -1, max) == max - 1);
    CHECK(MoveSelection(max - 1, max, max) == max);
    CHECK(MoveSelection(min, min, max) == 0);
    CHECK(MoveSelection(max, max, min) == 0);
    CHECK(PageFirst(max, max) == (max - 1) / 6 * 6);
    CHECK(PageFirst(max, max - 1) == (max - 2) / 6 * 6);
    CHECK(PageFirst(min, max) == 0);
    CHECK(PageFirst(max, min) == 0);
}

static constexpr std::array<PendingStage, 4> stages{
    PendingStage::None, PendingStage::OpenSetup, PendingStage::OpenDrivers, PendingStage::Launch};

static void CheckPending() {
    PendingFlow initial;
    CHECK(initial.stage == PendingStage::None);
    for (auto before : stages) {
        PendingFlow reset;
        reset.stage = before;
        reset.Reset();
        CHECK(reset.stage == PendingStage::None);
        reset.Reset();
        CHECK(reset.stage == PendingStage::None);
        for (auto queued : stages) {
            for (bool enabled : {false, true}) {
                PendingFlow flow;
                flow.stage = before;
                flow.Queue(queued, enabled);
                CHECK(flow.stage == (enabled ? queued : PendingStage::None));
            }
        }
        for (bool enabled : {false, true}) {
            for (bool inLobby : {false, true}) {
                for (bool canStart : {false, true}) {
                    PendingFlow flow;
                    flow.stage = before;
                    const bool consumed = enabled && inLobby &&
                        (before == PendingStage::OpenSetup || before == PendingStage::OpenDrivers ||
                         (before == PendingStage::Launch && canStart));
                    CHECK(flow.Take(enabled, inLobby, canStart) == (consumed ? before : PendingStage::None));
                    const auto after = !enabled || consumed ? PendingStage::None : before;
                    CHECK(flow.stage == after);
                    CHECK(flow.Take(enabled, inLobby, canStart) == PendingStage::None);
                    CHECK(flow.stage == after);
                    // Every possible subsequent observation, including re-entry to Lobby.
                    for (bool nextEnabled : {false, true}) {
                        for (bool nextLobby : {false, true}) {
                            for (bool nextCanStart : {false, true}) {
                                auto next = flow;
                                const bool nextConsumed = nextEnabled && nextLobby &&
                                    (after == PendingStage::OpenSetup || after == PendingStage::OpenDrivers ||
                                     (after == PendingStage::Launch && nextCanStart));
                                CHECK(next.Take(nextEnabled, nextLobby, nextCanStart) ==
                                    (nextConsumed ? after : PendingStage::None));
                                CHECK(next.stage == (!nextEnabled || nextConsumed ? PendingStage::None : after));
                            }
                        }
                    }
                    // Error/disconnect wins over routing and even a ready-to-start Lobby.
                    flow.stage = before;
                    CHECK(flow.Take(enabled, inLobby, canStart, true) == PendingStage::None);
                    CHECK(flow.stage == PendingStage::None);
                    CHECK(flow.Take(true, true, true) == PendingStage::None);
                }
            }
        }
    }
}

static void CheckStandingsHandoff() {
    for (bool canStart : {false, true}) {
        PendingFlow pending;
        // Results/NEXT RACE never schedules an automatic menu transition or launch.
        CHECK(pending.Take(true, false, canStart) == PendingStage::None);
        CHECK(pending.Take(true, true, canStart) == PendingStage::None);
        // SETUP queues only the route; NET_RETURN_LOBBY remains a parent action.
        pending.Queue(PendingStage::OpenSetup, true);
        for (int frame = 0; frame < 4; ++frame) {
            CHECK(pending.Take(true, false, canStart) == PendingStage::None);
            CHECK(pending.stage == PendingStage::OpenSetup);
        }
        CHECK(pending.Take(true, true, canStart) == PendingStage::OpenSetup);
        CHECK(pending.Take(true, true, true) == PendingStage::None);
        // Disconnect while returning drops the route; reconnect does not resurrect it.
        pending.Queue(PendingStage::OpenSetup, true);
        CHECK(pending.Take(true, false, canStart, true) == PendingStage::None);
        CHECK(pending.Take(true, true, true) == PendingStage::None);
    }
}

static void CheckDriverHandoff() {
    using GameUI::DriverSelect::Flow;
    using GameUI::DriverSelect::Step;
    for (int port = 0; port < 4; ++port) {
        const bool enabled = Enabled(true, true, 1);
        PendingFlow pending;
        Flow drivers;
        // NEXT prepares the local session before opening the shared character stage.
        pending.Queue(PendingStage::OpenDrivers, enabled);
        CHECK(pending.Take(enabled, false, true) == PendingStage::None);
        CHECK(pending.Take(enabled, true, false) == PendingStage::OpenDrivers);
        drivers.Reset(1u << port);
        CHECK(drivers.step == Step::Character);
        CHECK(pending.Take(enabled, true, false) == PendingStage::None);
        // Back from an unconfirmed character returns to setup in the parent.
        CHECK(!drivers.Back(port));
        pending.Reset();
        CHECK(pending.Take(enabled, true, true) == PendingStage::None);
        // Re-enter, confirm character, and return from transmission to character.
        pending.Queue(PendingStage::OpenDrivers, enabled);
        CHECK(pending.Take(enabled, true, false) == PendingStage::OpenDrivers);
        drivers.Reset(1u << port);
        CHECK(drivers.ConfirmCharacter(port));
        CHECK(drivers.step == Step::Transmission);
        CHECK(drivers.Back(port));
        CHECK(drivers.step == Step::Character);
        CHECK(pending.Take(enabled, true, true) == PendingStage::None);
        CHECK(drivers.ConfirmCharacter(port));
        CHECK(drivers.ConfirmTransmission(port));
        CHECK(drivers.step == Step::Ready);
        // The parent readies here without drawing Ready or the one-person lobby.
        pending.Queue(PendingStage::Launch, enabled);
        CHECK(pending.Take(enabled, true, false) == PendingStage::None);
        CHECK(pending.Take(enabled, true, true) == PendingStage::Launch);
        CHECK(pending.Take(enabled, true, true) == PendingStage::None);
        CHECK(pending.Take(enabled, false, false) == PendingStage::None);
        // A start error reopens selection only through an explicit new request.
        pending.Reset();
        CHECK(pending.Take(enabled, true, true) == PendingStage::None);
        pending.Queue(PendingStage::OpenDrivers, enabled);
        CHECK(pending.Take(enabled, true, true) == PendingStage::OpenDrivers);
        drivers.Reset(1u << port);
        CHECK(drivers.step == Step::Character);
        CHECK(!drivers.characterLocked[port] && !drivers.transmissionLocked[port]);
        CHECK(drivers.ConfirmCharacter(port));
        CHECK(drivers.ConfirmTransmission(port));
        pending.Queue(PendingStage::Launch, enabled);
        CHECK(pending.Take(enabled, true, true) == PendingStage::Launch);
        // Back during a queued launch cancels it even if readiness arrives later.
        pending.Queue(PendingStage::Launch, enabled);
        pending.Reset();
        CHECK(pending.Take(enabled, true, true) == PendingStage::None);
    }
    // A mode/party change permanently drops work, even if eligibility later returns.
    for (auto stage : stages) {
        for (bool explicitSolo : {false, true}) {
            for (bool localOnly : {false, true}) {
                for (int count = 0; count <= 4; ++count) {
                    if (explicitSolo && localOnly && count == 1) continue;
                    PendingFlow pending;
                    pending.Queue(stage, true);
                    CHECK(pending.Take(Enabled(explicitSolo, localOnly, count), false, false) == PendingStage::None);
                    CHECK(pending.stage == PendingStage::None);
                    CHECK(pending.Take(true, true, true) == PendingStage::None);
                }
            }
        }
    }
}

int main() {
    CheckEligibility();
    CheckNavigation();
    CheckPending();
    CheckStandingsHandoff();
    CheckDriverHandoff();
    std::printf("%d solo menu checks passed\n", checks);
}
