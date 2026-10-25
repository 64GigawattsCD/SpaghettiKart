#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace KartNet {
constexpr size_t MaxRacers = 8, MaxLocalPlayers = 4;
struct PartyPlayer {
    uint8_t controller = 0, character = 0, transmission = 0;
    bool ready = false;
};
using Party = std::vector<PartyPlayer>;
struct Identity {
    uint16_t participant = 0, connection = 0;
    uint8_t racer = 0, controller = 0, character = 0, transmission = 0;
    bool ready = false, connected = true;
};
struct RaceSettings {
    // Modes: GP, VS, Battle. Scope: local-only, LAN, direct Internet.
    uint8_t mode = 0, scope = 1, maxPlayers = 8, cup = 0, course = 0, cc = 0, laps = 3;
    uint8_t round = 0, items = 0, balloons = 3;
    bool cpuFill = false, mirror = false;
    std::string name = "Spaghetti Kart";
    bool Valid() const {
        return mode <= 2 && scope <= 2 && maxPlayers >= 1 && maxPlayers <= MaxRacers && cup < 4 &&
               course < 20 && (mode != 1 || course < 16) && (mode != 2 || course >= 16) &&
               cc < 4 && laps >= 1 && laps <= 9 && round < 4 && items <= 2 &&
               balloons >= 1 && balloons <= 5 && !name.empty() && name.size() <= 47 &&
               std::all_of(name.begin(), name.end(), [](unsigned char c) { return c >= 32 && c <= 126; });
    }
    int CourseIndex() const { return mode == 0 ? cup * 4 + round : course; }
};
inline bool ValidParty(const Party& party) {
    if (party.empty() || party.size() > MaxLocalPlayers) return false;
    unsigned controllers = 0;
    for (const auto& p : party) {
        if (p.controller >= MaxLocalPlayers || p.character >= 8 || p.transmission > 1 ||
            (controllers & (1u << p.controller))) return false;
        controllers |= 1u << p.controller;
    }
    return true;
}
inline bool ValidRoster(const std::vector<Identity>& roster) {
    if (roster.empty() || roster.size() > MaxRacers) return false;
    unsigned racers = 0, characters = 0;
    std::array<unsigned, 9> controllers{};
    for (const auto& p : roster) {
        if (p.participant != p.racer + 1 || p.racer >= MaxRacers || p.connection < 1 || p.connection > 8 ||
            p.controller >= MaxLocalPlayers || p.character >= 8 || p.transmission > 1 ||
            (racers & (1u << p.racer)) || (characters & (1u << p.character)) ||
            (controllers[p.connection] & (1u << p.controller))) return false;
        racers |= 1u << p.racer; characters |= 1u << p.character;
        controllers[p.connection] |= 1u << p.controller;
    }
    for (auto mask : controllers) if (mask && (mask & (mask + 1))) return false;
    return true;
}
inline std::vector<uint16_t> EngineOrder(const std::vector<Identity>& roster, uint16_t connection) {
    std::vector<Identity> sorted = roster;
    std::sort(sorted.begin(), sorted.end(), [connection](const Identity& a, const Identity& b) {
        if ((a.connection == connection) != (b.connection == connection)) return a.connection == connection;
        if (a.connection == connection) return a.controller < b.controller;
        return a.racer < b.racer;
    });
    std::vector<uint16_t> result;
    for (const auto& p : sorted) result.push_back(p.participant);
    return result;
}
inline int EngineSlot(const std::vector<Identity>& roster, uint16_t connection, uint16_t participant) {
    auto order = EngineOrder(roster, connection);
    auto found = std::find(order.begin(), order.end(), participant);
    return found == order.end() ? -1 : int(found - order.begin());
}
}
