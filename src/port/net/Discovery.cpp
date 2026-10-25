#include "Discovery.h"
#include <enet/enet.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <limits>

namespace KartNet {
namespace {
uint64_t Now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
uint64_t Nonce() {
    static std::atomic<uint64_t> next{uint64_t(std::chrono::steady_clock::now().time_since_epoch().count()) | 1};
    return ++next;
}
bool ValidServer(const ServerInfo& s) {
    return s.session && s.port && s.mode <= 2 && s.cup < 4 && s.course < 20 &&
        s.capacity >= 1 && s.capacity <= 8 && s.players <= s.capacity &&
        int(s.phase) >= int(Phase::Offline) && int(s.phase) <= int(Phase::Error) &&
        !s.name.empty() && s.name.size() <= 47 &&
        std::all_of(s.name.begin(), s.name.end(), [](unsigned char c) { return c >= 32 && c <= 126; });
}
void Put(std::vector<uint8_t>& out, uint64_t n, size_t width) {
    for (size_t i = width; i; --i) out.push_back(uint8_t(n >> ((i - 1) * 8)));
}
uint64_t Get(const uint8_t* bytes, size_t& offset, size_t width) {
    uint64_t n = 0;
    for (size_t i = 0; i < width; ++i) n = (n << 8) | bytes[offset++];
    return n;
}
}
namespace DiscoveryDetail {
std::vector<uint8_t> Encode(const Packet& p) {
    if (!p.query || (p.kind != PacketKind::Query && p.kind != PacketKind::Reply) ||
        (p.kind == PacketKind::Reply && !ValidServer(p.server))) return {};
    std::vector<uint8_t> out{'S', 'K', 'L', 'D', uint8_t(p.kind)};
    Put(out, p.protocol, 2); Put(out, p.rules, 4); Put(out, p.query, 8);
    if (p.kind == PacketKind::Reply) {
        const auto& s = p.server;
        Put(out, s.session, 8); Put(out, s.content, 8); Put(out, s.port, 2);
        out.insert(out.end(), {s.mode, s.cup, s.course, s.players, s.capacity,
                              uint8_t(s.locked), uint8_t(s.phase), uint8_t(s.name.size())});
        out.insert(out.end(), s.name.begin(), s.name.end());
    }
    return out;
}
bool Decode(const uint8_t* bytes, size_t size, Packet& result) {
    if (!bytes || size < 19 || size > PacketLimit || bytes[0] != 'S' || bytes[1] != 'K' ||
        bytes[2] != 'L' || bytes[3] != 'D' || (bytes[4] != 1 && bytes[4] != 2)) return false;
    Packet p;
    p.kind = PacketKind(bytes[4]);
    size_t i = 5;
    p.protocol = uint16_t(Get(bytes, i, 2)); p.rules = uint32_t(Get(bytes, i, 4));
    p.query = Get(bytes, i, 8);
    if (!p.query) return false;
    if (p.kind == PacketKind::Query) {
        if (size != i) return false;
    } else {
        if (size < 45) return false;
        auto& s = p.server;
        s.session = Get(bytes, i, 8); s.content = Get(bytes, i, 8); s.port = uint16_t(Get(bytes, i, 2));
        s.mode = bytes[i++]; s.cup = bytes[i++]; s.course = bytes[i++];
        s.players = bytes[i++]; s.capacity = bytes[i++];
        if (bytes[i] > 1) return false;
        s.locked = bytes[i++] != 0; s.phase = Phase(bytes[i++]);
        const size_t length = bytes[i++];
        if (length == 0 || length > 47 || size != i + length) return false;
        s.name.assign(reinterpret_cast<const char*>(bytes + i), length);
        if (!ValidServer(s)) return false;
    }
    result = std::move(p);
    return true;
}
bool HasPartyCapacity(const ServerInfo& s, uint8_t partySize) {
    return partySize >= 1 && partySize <= MaxLocalPlayers && s.capacity >= 1 && s.capacity <= 8 &&
           s.players <= s.capacity && partySize <= s.capacity - s.players;
}
void Cache::Clear() { servers.clear(); seen.clear(); }
void Cache::Expire(uint64_t now) {
    for (size_t i = servers.size(); i-- > 0;) {
        if (now >= seen[i] && now - seen[i] >= LifetimeMs) {
            servers.erase(servers.begin() + i); seen.erase(seen.begin() + i);
        }
    }
}
bool Cache::Update(const Packet& p, const std::string& address, uint64_t content,
                   uint64_t ownSession, uint64_t now, uint32_t ping) {
    Expire(now);
    if (p.kind != PacketKind::Reply || !ValidServer(p.server) || p.server.session == ownSession ||
        address.empty() || address.size() > 15) return false;
    auto s = p.server;
    s.address = address; s.ping = ping;
    s.compatible = p.protocol == ProtocolVersion && p.rules == RulesVersion && s.content == content;
    for (size_t i = 0; i < servers.size(); ++i) {
        if (servers[i].session == s.session && servers[i].address == s.address && servers[i].port == s.port) {
            servers[i] = std::move(s); seen[i] = now; return true;
        }
    }
    if (servers.size() >= ServerLimit) return false;
    servers.push_back(std::move(s)); seen.push_back(now); return true;
}
}

struct Discovery::Impl {
    ENetSocket browser = ENET_SOCKET_NULL, listener = ENET_SOCKET_NULL;
    bool initialized = false;
    uint64_t content = 0, query = 0, sentAt = 0, ownSession = 0;
    std::string error;
    DiscoveryDetail::Cache cache;
    DiscoveryDetail::Packet advertisement;
    ~Impl() {
        Close(browser); Close(listener);
        if (initialized) enet_deinitialize();
    }
    static void Close(ENetSocket& socket) {
        if (socket != ENET_SOCKET_NULL) enet_socket_destroy(socket);
        socket = ENET_SOCKET_NULL;
    }
    bool Open(ENetSocket& socket, uint16_t port, bool broadcast) {
        if (socket != ENET_SOCKET_NULL) return true;
        if (!initialized) {
            if (enet_initialize() != 0) { error = "Discovery initialization failed"; return false; }
            initialized = true;
        }
        socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        ENetAddress address{ENET_HOST_ANY, port};
        if (socket == ENET_SOCKET_NULL || enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1) < 0 ||
            enet_socket_set_option(socket, ENET_SOCKOPT_REUSEADDR, 1) < 0 ||
            (broadcast && enet_socket_set_option(socket, ENET_SOCKOPT_BROADCAST, 1) < 0) ||
            enet_socket_bind(socket, &address) < 0) {
            error = "Discovery UDP socket unavailable"; Close(socket); return false;
        }
        return true;
    }
    bool Send(ENetSocket socket, const ENetAddress& to, const DiscoveryDetail::Packet& packet) {
        auto bytes = DiscoveryDetail::Encode(packet);
        if (bytes.empty()) return false;
        ENetBuffer buffer{};
        buffer.data = bytes.data(); buffer.dataLength = bytes.size();
        return enet_socket_send(socket, &to, &buffer, 1) == int(bytes.size());
    }
};
Discovery::Discovery() : impl(new Impl) {}
Discovery::~Discovery() = default;
void Discovery::Refresh(uint64_t content, uint8_t partySize) {
    auto& d = *impl;
    d.error.clear(); d.cache.Clear(); d.content = content;
    if (partySize < 1 || partySize > MaxLocalPlayers) { d.error = "Party size must be 1 to 4"; return; }
    if (!d.Open(d.browser, 0, true)) return;
    d.query = Nonce(); d.sentAt = Now();
    DiscoveryDetail::Packet query; query.query = d.query;
    ENetAddress broadcast{ENET_HOST_BROADCAST, DiscoveryPort}, loopback{};
    loopback.port = DiscoveryPort;
    const bool sentBroadcast = d.Send(d.browser, broadcast, query);
    const bool sentLoopback = enet_address_set_host_ip(&loopback, "127.0.0.1") == 0 && d.Send(d.browser, loopback, query);
    if (!sentBroadcast && !sentLoopback) d.error = "Discovery query send failed";
}
void Discovery::Advertise(uint16_t port, uint64_t session, uint64_t content, const RaceSettings& settings,
                          uint8_t players, bool locked, Phase phase) {
    auto& d = *impl;
    if (d.ownSession != session) d.cache.Clear();
    d.ownSession = session;
    if (settings.scope != 1) { StopAdvertising(); return; }
    ServerInfo s;
    s.name = settings.name; s.port = port; s.session = session; s.content = content;
    s.mode = settings.mode; s.cup = settings.cup; s.course = settings.course;
    s.players = players; s.capacity = settings.maxPlayers; s.locked = locked; s.phase = phase;
    if (!settings.Valid() || !ValidServer(s)) {
        StopAdvertising(); d.error = "Invalid discovery lobby settings"; return;
    }
    d.advertisement.kind = DiscoveryDetail::PacketKind::Reply;
    d.advertisement.server = std::move(s);
    d.Open(d.listener, DiscoveryPort, false);
}
void Discovery::StopAdvertising() { Impl::Close(impl->listener); }
const std::vector<ServerInfo>& Discovery::Servers() const { return impl->cache.Servers(); }
const std::string& Discovery::Error() const { return impl->error; }
void Discovery::Poll() {
    auto& d = *impl;
    const uint64_t now = Now();
    d.cache.Expire(now);
    // A single shared budget covers both sockets, including malformed datagrams.
    for (unsigned count = 0; count < 64; ++count) {
        bool received = false;
        for (int side = 0; side < 2; ++side) {
            const auto socket = side == 0 ? d.listener : d.browser;
            if (socket == ENET_SOCKET_NULL) continue;
            std::array<uint8_t, DiscoveryDetail::PacketLimit + 1> bytes{};
            ENetBuffer buffer{};
            buffer.data = bytes.data(); buffer.dataLength = bytes.size();
            ENetAddress source{};
            const int length = enet_socket_receive(socket, &source, &buffer, 1);
            if (length < 0) { d.error = "Discovery UDP receive failed"; continue; }
            if (!length) continue;
            received = true;
            DiscoveryDetail::Packet packet;
            if (DiscoveryDetail::Decode(bytes.data(), size_t(length), packet)) {
                if (side == 0 && packet.kind == DiscoveryDetail::PacketKind::Query) {
                    auto reply = d.advertisement; reply.query = packet.query;
                    if (!d.Send(socket, source, reply)) d.error = "Discovery reply send failed";
                } else if (side == 1 && packet.kind == DiscoveryDetail::PacketKind::Reply &&
                           d.query && packet.query == d.query && now >= d.sentAt && now - d.sentAt < DiscoveryDetail::LifetimeMs) {
                    char address[16]{};
                    if (enet_address_get_host_ip(&source, address, sizeof(address)) == 0)
                        d.cache.Update(packet, address, d.content, d.ownSession, now,
                            uint32_t(std::min<uint64_t>(now - d.sentAt, (std::numeric_limits<uint32_t>::max)())));
                }
            }
            break;
        }
        if (!received) break;
    }
}
}
