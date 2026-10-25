#include "../../src/port/ui/DriverSelectModel.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <queue>
#include <set>

using namespace GameUI::DriverSelect;

static int checks = 0;
static void Require(bool condition, int line) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "Driver select check %d failed at line %d\n", checks, line);
        std::abort();
    }
}
#define CHECK(condition) Require((condition), __LINE__)

static bool SameRect(Rect a, Rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static bool Contains(Rect outer, Rect inner) {
    return inner.w > 0 && inner.h > 0 && inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.w <= outer.x + outer.w && inner.y + inner.h <= outer.y + outer.h;
}

static bool Overlaps(Rect a, Rect b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

template <size_t N>
static void CheckButtons(Rect view, const std::array<Rect, N>& buttons) {
    for (size_t i = 0; i < N; ++i) {
        CHECK(Contains(view, buttons[i]));
        for (size_t j = 0; j < i; ++j) CHECK(!Overlaps(buttons[i], buttons[j]));
    }
}

static void CheckGeometry() {
    const Rect screen{0, 0, 320, 240};
    for (int character = 0; character < 8; ++character) {
        const auto tile = CharacterTile(character);
        CHECK(SameRect(tile, {16 + 74 * (character % 4), 43 + 72 * (character / 4), 66, 66}));
        CHECK(Contains(screen, tile));
        for (int other = 0; other < character; ++other) CHECK(!Overlaps(tile, CharacterTile(other)));
    }
    for (int count = 1; count <= 4; ++count) {
        for (int index = 0; index < count; ++index) {
            const auto view = Viewport(count, index);
            const Rect expected = count == 1 ? screen : count == 2 ? Rect{0, 120 * index, 320, 120}
                : Rect{160 * (index % 2), 120 * (index / 2), 160, 120};
            CHECK(SameRect(view, expected));
            CHECK(Contains(screen, view));
            for (int other = 0; other < index; ++other) CHECK(!Overlaps(view, Viewport(count, other)));
            const int cx = view.x + view.w / 2, cy = view.y + view.h / 2;
            CHECK(SameRect(AutoButton(count, index), {cx - 68, cy - 11, 64, 26}));
            CHECK(SameRect(ManualButton(count, index), {cx + 4, cy - 11, 64, 26}));
            CHECK(SameRect(ConfirmButton(count, index), {cx - 26, cy + 23, 52, 24}));
            CHECK(SameRect(ReadyButton(count, index), {cx - 44, cy + 8, 88, 28}));
            CHECK(SameRect(BackButton(count, index), {view.x + 6, view.y + view.h - 22, 44, 16}));
            // Transmission and Ready occupy separate stages.
            CheckButtons(view, std::array<Rect, 4>{AutoButton(count, index), ManualButton(count, index),
                                                 ConfirmButton(count, index), BackButton(count, index)});
            CheckButtons(view, std::array<Rect, 2>{ReadyButton(count, index), BackButton(count, index)});
        }
    }
    // Every physical-port mask maps to compact viewports, including masks with gaps.
    for (unsigned mask = 1; mask < 16; ++mask) {
        int count = 0;
        for (int port = 0; port < 4; ++port) if (mask & (1u << port)) ++count;
        int index = 0;
        for (int port = 0; port < 4; ++port) {
            if (!(mask & (1u << port))) continue;
            CHECK(Contains(Viewport(count, index), ReadyButton(count, index)));
            CHECK(Contains(Viewport(count, index), AutoButton(count, index)));
            ++index;
        }
        CHECK(index == count);
    }
}

static unsigned LockBits(const std::array<bool, 4>& locks) {
    unsigned bits = 0;
    for (int port = 0; port < 4; ++port) if (locks[port]) bits |= 1u << port;
    return bits;
}

struct State {
    Step step = Step::Character;
    unsigned characters = 0, transmissions = 0;
};

static unsigned Key(State state) {
    return static_cast<unsigned>(state.step) | (state.characters << 2) | (state.transmissions << 6);
}

static void CheckState(const Flow& flow, unsigned mask, State state) {
    CHECK(flow.mask == mask);
    CHECK(flow.step == state.step);
    CHECK(LockBits(flow.characterLocked) == state.characters);
    CHECK(LockBits(flow.transmissionLocked) == state.transmissions);
}

// Independent bitset oracle for all actions from every reachable state.
static State Next(State state, unsigned mask, int action, int port) {
    if (port < 0 || port >= 4 || !(mask & (1u << port))) return state;
    const unsigned bit = 1u << port;
    if (action == 0 && state.step == Step::Character) {
        state.characters |= bit;
        if (state.characters == mask) state.step = Step::Transmission;
    } else if (action == 1 && state.step == Step::Transmission) {
        state.transmissions |= bit;
        if (state.transmissions == mask) state.step = Step::Ready;
    } else if (action == 2) {
        switch (state.step) {
            case Step::Ready:
                state.step = Step::Transmission;
                state.transmissions &= ~bit;
                break;
            case Step::Transmission:
                state.step = Step::Character;
                state.characters &= ~bit;
                state.transmissions = 0;
                break;
            case Step::Character:
                state.characters &= ~bit;
                break;
        }
    }
    return state;
}

static void CheckFlow() {
    Flow initial;
    CheckState(initial, 0, {});
    // Reset replaces both lock arrays and the stage, and strips every upper bit.
    for (unsigned joinedMask = 0; joinedMask < 256; ++joinedMask) {
        Flow flow;
        flow.mask = ~0u;
        flow.step = Step::Ready;
        flow.characterLocked.fill(true);
        flow.transmissionLocked.fill(true);
        flow.Reset(joinedMask);
        CheckState(flow, joinedMask & 15u, {});
    }
    initial.Reset(std::numeric_limits<unsigned>::max());
    CheckState(initial, 15, {});
    const std::array<int, 8> ports{-1, 0, 1, 2, 3, 4,
                                  std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
    for (unsigned mask = 0; mask < 16; ++mask) {
        Flow start;
        start.Reset(mask);
        std::queue<Flow> pending;
        std::set<unsigned> visited;
        pending.push(start);
        visited.insert(Key({}));
        std::array<bool, 3> reached{};
        while (!pending.empty()) {
            const auto flow = pending.front();
            pending.pop();
            const State before{flow.step, LockBits(flow.characterLocked), LockBits(flow.transmissionLocked)};
            reached[static_cast<unsigned>(flow.step)] = true;
            CHECK((before.characters & ~mask) == 0);
            CHECK((before.transmissions & ~mask) == 0);
            if (flow.step != Step::Character) CHECK(before.characters == mask);
            if (flow.step == Step::Ready) CHECK(before.transmissions == mask);
            for (int action = 0; action < 3; ++action) {
                for (int port : ports) {
                    auto next = flow;
                    const auto expected = Next(before, mask, action, port);
                    const bool changed = action == 0 ? next.ConfirmCharacter(port)
                        : action == 1 ? next.ConfirmTransmission(port) : next.Back(port);
                    CheckState(next, mask, expected);
                    CHECK(changed == (Key(before) != Key(expected)));
                    if (visited.insert(Key(expected)).second) pending.push(next);
                }
            }
            for (unsigned nextMask = 0; nextMask < 16; ++nextMask) {
                auto reset = flow;
                reset.Reset(nextMask | 0xfffffff0u);
                CheckState(reset, nextMask, {});
            }
        }
        CHECK(reached[0]);
        CHECK(reached[1] == (mask != 0));
        CHECK(reached[2] == (mask != 0));
        // Each barrier admits every proper subset of its locks, plus the Ready state.
        int count = 0;
        for (int port = 0; port < 4; ++port) if (mask & (1u << port)) ++count;
        CHECK(visited.size() == (mask == 0 ? 1u : 2u * ((1u << count) - 1u) + 1u));
    }
}

int main() {
    CheckGeometry();
    CheckFlow();
    std::printf("%d driver select checks passed\n", checks);
}
