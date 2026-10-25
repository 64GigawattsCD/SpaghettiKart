#include "../../src/port/ui/GameOptionsModel.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <queue>

using namespace GameUI::Lobby;
static int checks = 0;
static void Require(bool condition) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "Lobby check %d failed\n", checks); std::abort(); }
}
int main() {
    const auto overlaps = [](MenuRect a, MenuRect b) {
        return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
    };
    const MenuRect footer{16, 202, 288, 28}, title{60, 47, 200, 32};
    for (int player = 0; player < 4; ++player) {
        const auto badge = MainPlayerRect(player);
        Require(badge.x >= 16 && badge.x + badge.w <= 304 && badge.y >= 14);
        Require(!overlaps(badge, footer) && !overlaps(badge, title));
        for (int other = 0; other < player; ++other) Require(!overlaps(badge, MainPlayerRect(other)));
        for (int mode = 0; mode < 3; ++mode) Require(!overlaps(badge, MainModeRect(mode)));
    }
    for (int mode = 0; mode < 3; ++mode) {
        const auto tile = MainModeRect(mode);
        Require(tile.w == 64 && tile.h == 54);
        Require(!overlaps(tile, title) && !overlaps(tile, footer));
        for (int other = 0; other < mode; ++other) Require(!overlaps(tile, MainModeRect(other)));
    }
    for (unsigned bits = 0; bits < 256; ++bits) {
        const bool joined = bits & 1, lobby = bits & 2;
        const auto action = ResolvePartyAction(joined, lobby, bits & 4, bits & 8, bits & 16, bits & 32, bits & 64);
        if (!lobby) Require(action == PartyAction::None || action == PartyAction::Join || action == PartyAction::Leave);
        if (!joined) Require(action == (bits & 4 ? PartyAction::Join : PartyAction::None));
    }
    Require(ResolvePartyAction(true, true, false, false, false, true, false) == PartyAction::Character);
    Require(ResolvePartyAction(true, true, false, false, false, false, true) == PartyAction::Transmission);
    Require(ResolvePartyAction(true, true, true, false, false, false, false) == PartyAction::Ready);
    for (int direction : {-1, 1}) {
        Require(SetupModeDirection(0, direction, true) == 1);
        Require(SetupModeDirection(1, direction, true) == -1);
        Require(SetupModeDirection(2, direction, true) == -2);
        Require(SetupModeDirection(0, direction, false) == direction);
        Require(SetupModeDirection(1, direction, false) == direction);
    }
    Require(PartyFits(4, 4, 8));
    Require(!PartyFits(4, 5, 8));
    Require(!PartyFits(0, 0, 8));
    Require(!PartyFits(5, 0, 8));
    Require(!PartyFits(1, 0, 9));
    for (int party = -1; party <= 5; ++party)
        for (int occupied = -1; occupied <= 9; ++occupied)
            for (int capacity = 0; capacity <= 9; ++capacity) {
                bool expected = party >= 1 && party <= 4 && occupied >= 0 && capacity <= 8 && occupied + party <= capacity;
                Require(PartyFits(party, occupied, capacity) == expected);
                for (int phase = 0; phase <= 7; ++phase) {
                    Require(BrowserJoinable(party, occupied, capacity, true, phase) == (expected && phase == 2));
                    Require(!BrowserJoinable(party, occupied, capacity, false, phase));
                }
            }
    Require(Wrap(-1, 4) == 3);
    Require(Wrap(4, 4) == 0);
    Require(Wrap(5, 0) == 0);
    Require(PageFirst(5) == 0 && PageFirst(6) == 6 && PageFirst(11) == 6 && PageFirst(12) == 12);
    Require(KeyboardMove(0, -1, 0) == 9);
    Require(KeyboardMove(9, 1, 0) == 0);
    Require(KeyboardMove(40, -1, 0) == 43);
    Require(KeyboardMove(43, 1, 0) == 40);
    Require(KeyboardMove(39, 0, 1) == 43);
    Require(KeyboardMove(43, 0, -1) == 39);
    std::array<bool, 44> reachable{};
    std::queue<int> pending;
    reachable[0] = true; pending.push(0);
    while (!pending.empty()) {
        int key = pending.front(); pending.pop();
        for (auto direction : std::array<std::array<int, 2>, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
            int next = KeyboardMove(key, direction[0], direction[1]);
            Require(next >= 0 && next < 44);
            if (!reachable[next]) { reachable[next] = true; pending.push(next); }
        }
    }
    for (bool reached : reachable) Require(reached);
    for (int key : {26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 40, 43}) {
        for (auto direction : std::array<std::array<int, 2>, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
            int next = PortKeyboardMove(key, direction[0], direction[1]);
            Require(next == 26 || (next >= 27 && next <= 35) || next == 40 || next == 43);
        }
    }
    Require(PortKeyboardMove(26, -1, 0) == 40);
    Require(PortKeyboardMove(26, 1, 0) == 43);
    Require(PortKeyboardMove(27, 0, -1) == 40);
    for (size_t size = 1; size < 65; ++size) {
        auto hidden = HiddenText(size);
        Require(hidden.size() == std::min<size_t>(size, 16));
        Require(hidden.find_first_not_of('-') == std::string::npos);
    }
    Require(HiddenText(0) == "NONE");
    for (int port = 0; port <= 65536; ++port) Require(ValidPort(std::to_string(port)) == (port >= 1 && port <= 65535));
    for (const char* invalid : {"", "-1", "+1", "123456", "1.0", "1:2", " 42", "42 ", "1A", "65536"}) Require(!ValidPort(invalid));
    Require(ValidPort("00001"));
    std::printf("%d lobby checks passed\n", checks);
}
