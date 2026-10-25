#pragma once
#include "Roster.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace KartNet {
constexpr uint16_t ProtocolVersion = 4;
constexpr uint32_t RulesVersion = 0x20261025;
constexpr size_t MaxPacket = 1200, KartsPerPacket = 4;
constexpr uint32_t DrivingButtonMask = 0x37ffef30;
enum class Kind : uint8_t { Hello = 1, Welcome, Load, Loaded, Begin, Input, Snapshot, Finish,
                          Lobby, Ready, Reject, UpdateParty, World };
enum class Phase { Offline, Connecting, Lobby, Loading, Racing, Finished, Disconnected, Error };

// Local controller/view indices never travel as global racer indices.
int LocalSlot(uint16_t participant, uint16_t localParticipant);
bool Newer(uint32_t next, uint32_t previous);

struct Input {
    uint32_t sequence = 0, buttons = 0;
    uint16_t participant = 0;
    int8_t x = 0, y = 0;
    uint8_t throttle = 0, brake = 0;
};
struct Kart {
    uint16_t participant = 0, type = 0;
    uint32_t effects = 0;
    std::array<float, 3> pos{}, oldPos{}, velocity{};
    std::array<int16_t, 3> rotation{};
    std::array<float, 9> orientation{};
    std::array<float, 12> tyres{};
    std::array<float, 4> ground{};
    float speed = 0, rpm = 0, size = 1;
    int32_t steering = 0;
    int16_t driftYaw = 0, slope = 0;
    int8_t gear = 0;
    uint8_t lap = 0, rank = 0;
    std::array<uint32_t, 3> lapTimes{};
};
struct Message {
    Kind kind = Kind::Hello;
    uint64_t session = 0, content = 0;
    uint32_t rules = RulesVersion, tick = 0, acknowledgedInput = 0, revision = 0, race = 0;
    uint32_t partyRevision = 0;
    uint16_t connection = 0;
    uint8_t character = 0, round = 0, part = 0, parts = 1, controller = 255;
    bool ready = false, cupComplete = false;
    Phase phase = Phase::Lobby;
    std::array<uint8_t, 16> ticket{};
    float time = 0;
    std::string reason;
    std::vector<uint8_t> payload;
    RaceSettings settings;
    Party party;
    Input input;
    std::vector<Identity> roster;
    std::vector<Kart> karts = std::vector<Kart>(2);
    std::array<uint16_t, MaxRacers> standings{};
};
std::vector<uint8_t> Encode(const Message& message);
bool Decode(const uint8_t* data, size_t length, Message& result);
}
