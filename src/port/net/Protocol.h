#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace KartNet {
constexpr uint16_t ProtocolVersion = 2;
constexpr uint32_t RulesVersion = 0x20261016;
constexpr size_t MaxPacket = 1200;
enum class Kind : uint8_t { Hello = 1, Welcome, Load, Loaded, Begin, Input, Snapshot, Finish };
enum class Phase { Offline, Connecting, Lobby, Loading, Racing, Finished, Disconnected, Error };

struct Identity {
    uint16_t participant = 0, connection = 0;
    uint8_t racer = 0, controller = 0, character = 0;
};
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
    float speed = 0, rpm = 0;
    int32_t steering = 0;
    int16_t driftYaw = 0, slope = 0;
    int8_t gear = 0;
    uint8_t lap = 0, rank = 0;
    std::array<uint32_t, 3> lapTimes{};
};
struct Message {
    Kind kind = Kind::Hello;
    uint64_t session = 0, content = 0;
    uint32_t rules = RulesVersion, tick = 0, acknowledgedInput = 0;
    uint8_t character = 0;
    float time = 0;
    Input input;
    std::array<Identity, 2> roster{};
    std::array<Kart, 2> karts{};
};
std::vector<uint8_t> Encode(const Message& message);
bool Decode(const uint8_t* data, size_t length, Message& result);
}
