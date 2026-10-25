#pragma once
#include "Protocol.h"
#include <memory>

namespace KartNet {
constexpr uint16_t DiscoveryPort = 54778, DefaultGamePort = 54777;
struct ServerInfo {
    std::string name, address;
    uint16_t port = DefaultGamePort;
    uint64_t session = 0, content = 0;
    uint8_t mode = 0, cup = 0, course = 0, players = 0, capacity = 8;
    bool locked = false, compatible = false;
    uint32_t ping = 0;
    Phase phase = Phase::Lobby;
};

// Pure packet/cache helpers also used by offline tests. Times are monotonic milliseconds.
namespace DiscoveryDetail {
constexpr size_t PacketLimit = 256, ServerLimit = 64;
constexpr uint64_t LifetimeMs = 5000;
enum class PacketKind : uint8_t { Query = 1, Reply = 2 };
struct Packet {
    PacketKind kind = PacketKind::Query;
    uint16_t protocol = ProtocolVersion;
    uint32_t rules = RulesVersion;
    uint64_t query = 0;
    ServerInfo server;
};
std::vector<uint8_t> Encode(const Packet& packet);
bool Decode(const uint8_t* bytes, size_t size, Packet& packet);
bool HasPartyCapacity(const ServerInfo& server, uint8_t partySize);
class Cache {
  public:
    bool Update(const Packet& packet, const std::string& address, uint64_t content,
                uint64_t ownSession, uint64_t now, uint32_t ping);
    void Expire(uint64_t now);
    void Clear();
    const std::vector<ServerInfo>& Servers() const { return servers; }
  private:
    std::vector<ServerInfo> servers;
    std::vector<uint64_t> seen;
};
}

class Discovery {
  public:
    Discovery();
    ~Discovery();
    Discovery(const Discovery&) = delete;
    Discovery& operator=(const Discovery&) = delete;
    void Refresh(uint64_t content, uint8_t partySize = 1);
    void Poll();
    void Advertise(uint16_t gamePort, uint64_t session, uint64_t content,
                   const RaceSettings& settings, uint8_t players, bool passwordRequired, Phase phase);
    void StopAdvertising();
    const std::vector<ServerInfo>& Servers() const;
    const std::string& Error() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
