#include "Discovery.h"
#include <enet/enet.h>
#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>

using namespace KartNet;
namespace D = KartNet::DiscoveryDetail;
namespace {
unsigned checks = 0;
void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
D::Packet Reply() {
    D::Packet p;
    p.kind = D::PacketKind::Reply; p.query = 0x0102030405060708ULL;
    auto& s = p.server;
    s.name = "Mushroom LAN"; s.session = 0x1122334455667788ULL;
    s.content = 0x8877665544332211ULL; s.port = DefaultGamePort;
    s.mode = 1; s.cup = 3; s.course = 19; s.players = 4; s.capacity = 8;
    s.locked = true; s.phase = Phase::Lobby;
    return p;
}
void Codec() {
    auto p = Reply();
    auto bytes = D::Encode(p);
    D::Packet out;
    Check(bytes.size() == 45 + p.server.name.size() && bytes.size() <= D::PacketLimit, "Reply length");
    Check(bytes[5] == 0 && bytes[6] == ProtocolVersion && bytes[11] == 1 && bytes[18] == 8 &&
          bytes[19] == 0x11 && bytes[26] == 0x88 && bytes[35] == uint8_t(DefaultGamePort >> 8) &&
          bytes[36] == uint8_t(DefaultGamePort), "Explicit big endian integers");
    Check(D::Decode(bytes.data(), bytes.size(), out), "Reply decode");
    Check(out.query == p.query && out.server.session == p.server.session && out.server.content == p.server.content &&
          out.server.port == p.server.port && out.server.name == p.server.name && out.server.locked &&
          out.server.mode == 1 && out.server.cup == 3 && out.server.course == 19 &&
          out.server.players == 4 && out.server.capacity == 8 && out.server.phase == Phase::Lobby, "Reply roundtrip");
    for (size_t n = 0; n < bytes.size(); ++n) {
        out.query = 999;
        Check(!D::Decode(bytes.data(), n, out) && out.query == 999, "Truncation leaves output unchanged");
    }
    auto bad = bytes; bad.push_back(0);
    Check(!D::Decode(bad.data(), bad.size(), out), "Trailing byte");
    Check(!D::Decode(nullptr, bytes.size(), out), "Null buffer");
    bad.resize(257);
    Check(!D::Decode(bad.data(), bad.size(), out), "Oversized packet");
    for (size_t i : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(4)}) {
        bad = bytes; bad[i] = 255;
        Check(!D::Decode(bad.data(), bad.size(), out), "Bad header");
    }
    const auto invalid = [&](size_t offset, uint8_t byte, const char* message) {
        auto b = bytes; b[offset] = byte;
        Check(!D::Decode(b.data(), b.size(), out), message);
    };
    invalid(37, 3, "Unknown mode"); invalid(38, 4, "Unknown cup"); invalid(39, 20, "Unknown course");
    invalid(40, 9, "Too many players"); invalid(40, 255, "Player overflow");
    invalid(41, 0, "Zero capacity"); invalid(41, 9, "Capacity overflow"); invalid(41, 3, "Players exceed capacity");
    invalid(42, 2, "Invalid locked bit"); invalid(43, 255, "Invalid phase");
    invalid(44, 0, "Empty name"); invalid(44, 48, "Overlong name"); invalid(44, 255, "Name length overflow");
    invalid(45, 0, "Embedded null"); invalid(45, 31, "Control in name"); invalid(45, 127, "Non ASCII name");
    bad = bytes; std::fill(bad.begin() + 11, bad.begin() + 19, uint8_t(0));
    Check(!D::Decode(bad.data(), bad.size(), out), "Zero query nonce");
    bad = bytes; std::fill(bad.begin() + 19, bad.begin() + 27, uint8_t(0));
    Check(!D::Decode(bad.data(), bad.size(), out), "Zero session nonce");
    bad = bytes; bad[35] = bad[36] = 0;
    Check(!D::Decode(bad.data(), bad.size(), out), "Zero game port");
    p.server.name.assign(47, 'X'); bytes = D::Encode(p);
    Check(bytes.size() == 92 && bytes.back() == 'X' && D::Decode(bytes.data(), bytes.size(), out) &&
          out.server.name == p.server.name, "Maximum unterminated length-prefixed ASCII name");
    p.server.name.push_back('X'); Check(D::Encode(p).empty(), "Encoder rejects overlong name");
    p = Reply(); p.server.name = std::string("LAN\0hidden", 10);
    Check(D::Encode(p).empty(), "Encoder rejects embedded null");
    p = Reply(); p.server.players = 0; p.server.capacity = 1;
    bytes = D::Encode(p); Check(D::Decode(bytes.data(), bytes.size(), out), "Zero occupants supported");
    p = {}; p.query = 1; bytes = D::Encode(p);
    Check(bytes.size() == 19 && D::Decode(bytes.data(), bytes.size(), out) && out.kind == D::PacketKind::Query,
          "Query roundtrip");
    for (size_t n = 0; n < bytes.size(); ++n) Check(!D::Decode(bytes.data(), n, out), "Query truncation");
    bytes.push_back(0); Check(!D::Decode(bytes.data(), bytes.size(), out), "Query trailing byte");
    p = Reply(); ++p.protocol; ++p.rules; bytes = D::Encode(p);
    Check(D::Decode(bytes.data(), bytes.size(), out) && out.protocol == p.protocol && out.rules == p.rules,
          "Version mismatches retained for incompatible listing");
    std::mt19937 random(6736);
    for (unsigned i = 0; i < 20000; ++i) {
        std::vector<uint8_t> fuzz(random() % 300);
        for (auto& b : fuzz) b = uint8_t(random());
        D::Decode(fuzz.data(), fuzz.size(), out);
    }
}
void Cache() {
    D::Cache cache;
    auto p = Reply();
    const uint64_t content = p.server.content;
    Check(cache.Update(p, "192.168.1.2", content, 0, 100, 12), "Insert");
    Check(cache.Servers().size() == 1 && cache.Servers()[0].compatible && cache.Servers()[0].ping == 12,
          "Compatibility and ping");
    p.server.players = 8;
    Check(cache.Update(p, "192.168.1.2", content, 0, 200, 13) && cache.Servers().size() == 1 &&
          cache.Servers()[0].players == 8 && cache.Servers()[0].compatible, "Dedup updates, full server still compatible");
    Check(!D::HasPartyCapacity(cache.Servers()[0], 1), "Full server has no seat");
    ++p.protocol;
    cache.Update(p, "192.168.1.2", content, 0, 300, 0);
    Check(!cache.Servers()[0].compatible, "Protocol mismatch incompatible");
    --p.protocol; ++p.rules;
    cache.Update(p, "192.168.1.2", content, 0, 300, 0);
    Check(!cache.Servers()[0].compatible, "Rules mismatch incompatible");
    --p.rules;
    cache.Update(p, "192.168.1.2", content + 1, 0, 300, 0);
    Check(!cache.Servers()[0].compatible, "Content mismatch incompatible");
    cache.Update(p, "192.168.1.3", content, 0, 300, 0);
    Check(cache.Servers().size() == 2, "Same port and session on different endpoints remain distinct");
    ++p.server.port; cache.Update(p, "192.168.1.2", content, 0, 300, 0);
    Check(cache.Servers().size() == 3, "Different game port distinct");
    ++p.server.session; cache.Update(p, "192.168.1.2", content, 0, 300, 0);
    Check(cache.Servers().size() == 4, "New session at same endpoint distinct");
    Check(!cache.Update(p, "192.168.1.2", content, p.server.session, 300, 0), "Own session excluded");
    p.kind = D::PacketKind::Query;
    Check(!cache.Update(p, "192.168.1.2", content, 0, 300, 0), "Query not cached");
    cache.Expire(5299); Check(cache.Servers().size() == 4, "TTL before expiry");
    cache.Expire(5300); Check(cache.Servers().empty(), "TTL at exactly five seconds");
    p = Reply();
    for (uint64_t i = 0; i < D::ServerLimit; ++i) {
        p.server.session = i + 1;
        Check(cache.Update(p, "127.0.0.1", content, 0, 1000, 0), "Cache bounded insertion");
    }
    ++p.server.session;
    Check(!cache.Update(p, "127.0.0.1", content, 0, 1000, 0) && cache.Servers().size() == D::ServerLimit,
          "Cache cap");
    --p.server.session; p.server.name = "Updated";
    Check(cache.Update(p, "127.0.0.1", content, 0, 1100, 0), "Existing entry updates at cap");
    cache.Expire(6000); Check(cache.Servers().size() == 1, "Refresh extends individual TTL");
    cache.Clear(); Check(cache.Servers().empty(), "Clear");
    for (unsigned capacity = 1; capacity <= 8; ++capacity) {
        for (unsigned players = 0; players <= capacity; ++players) {
            for (unsigned party = 0; party <= 5; ++party) {
                ServerInfo s; s.capacity = uint8_t(capacity); s.players = uint8_t(players);
                Check(D::HasPartyCapacity(s, uint8_t(party)) == (party >= 1 && party <= 4 && party <= capacity - players),
                      "Party seats independent of compatibility");
            }
        }
    }
    Discovery idle; idle.Poll(); idle.StopAdvertising();
    Check(idle.Servers().empty() && idle.Error().empty(), "Disconnected idle instance has no socket work");
}
void Loopback() {
    Check(enet_initialize() == 0, "Test ENet initialization");
    ENetSocket socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    Check(socket != ENET_SOCKET_NULL, "Loopback client socket");
    ENetAddress bind{}; enet_address_set_host_ip(&bind, "127.0.0.1");
    Check(enet_socket_bind(socket, &bind) == 0 && enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1) == 0,
          "Loopback client bind");
    {
        Discovery host;
        RaceSettings settings; settings.name = "Loopback"; settings.scope = 1;
        host.Advertise(DefaultGamePort, 123, 456, settings, 4, true, Phase::Lobby);
        Check(host.Error().empty(), "Loopback advertiser bind");
        D::Packet q; q.query = 123456;
        auto bytes = D::Encode(q);
        ENetBuffer buffer{}; buffer.data = bytes.data(); buffer.dataLength = bytes.size();
        ENetAddress target{}; target.port = DiscoveryPort; enet_address_set_host_ip(&target, "127.0.0.1");
        Check(enet_socket_send(socket, &target, &buffer, 1) == int(bytes.size()), "Loopback query send");
        bool found = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!found && std::chrono::steady_clock::now() < deadline) {
            host.Poll();
            uint8_t data[257]{}; buffer.data = data; buffer.dataLength = sizeof(data);
            ENetAddress from{};
            const int n = enet_socket_receive(socket, &from, &buffer, 1);
            D::Packet reply;
            if (n > 0 && D::Decode(data, size_t(n), reply)) {
                found = reply.query == q.query && reply.server.session == 123 && reply.server.content == 456 &&
                        reply.server.players == 4 && reply.server.locked && reply.server.port == DefaultGamePort;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(found, "Ephemeral query port receives advertisement");
        {
            Discovery second;
            settings.name = "Second host";
            second.Advertise(DefaultGamePort, 124, 456, settings, 4, false, Phase::Lobby);
            Check(second.Error().empty(), "Two advertisers reuse discovery port");
            Check(enet_socket_set_option(socket, ENET_SOCKOPT_BROADCAST, 1) == 0, "Loopback broadcast enabled");
            Check(enet_address_set_host_ip(&target, "127.255.255.255") == 0, "Loopback-only broadcast address");
            q.query = 123457; bytes = D::Encode(q); buffer.data = bytes.data(); buffer.dataLength = bytes.size();
            Check(enet_socket_send(socket, &target, &buffer, 1) == int(bytes.size()), "Loopback broadcast query");
            unsigned hosts = 0;
            const auto bothDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (hosts != 3 && std::chrono::steady_clock::now() < bothDeadline) {
                host.Poll(); second.Poll();
                uint8_t data[257]{}; buffer.data = data; buffer.dataLength = sizeof(data);
                ENetAddress from{};
                const int n = enet_socket_receive(socket, &from, &buffer, 1);
                D::Packet reply;
                if (n > 0 && D::Decode(data, size_t(n), reply) && reply.query == q.query) {
                    if (reply.server.session == 123) hosts |= 1;
                    if (reply.server.session == 124) hosts |= 2;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            Check(hosts == 3, "Loopback broadcast reaches both shared-port advertisers");
            settings.scope = 2;
            second.Advertise(DefaultGamePort, 124, 456, settings, 4, false, Phase::Lobby);
            settings.scope = 0;
            host.Advertise(DefaultGamePort, 123, 456, settings, 4, false, Phase::Lobby);
            q.query = 123458; bytes = D::Encode(q); buffer.data = bytes.data(); buffer.dataLength = bytes.size();
            Check(enet_socket_send(socket, &target, &buffer, 1) == int(bytes.size()), "Non-LAN query");
            bool response = false;
            const auto quietDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(75);
            while (std::chrono::steady_clock::now() < quietDeadline) {
                host.Poll(); second.Poll();
                uint8_t data[257]{}; buffer.data = data; buffer.dataLength = sizeof(data);
                ENetAddress from{};
                const int n = enet_socket_receive(socket, &from, &buffer, 1);
                D::Packet reply;
                if (n > 0 && D::Decode(data, size_t(n), reply) && reply.query == q.query) response = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            Check(!response, "Local and direct scopes do not advertise");
        }
        host.StopAdvertising();
    }
    // Discovery's destructor must not tear down this independently initialized socket runtime.
    ENetAddress actual{};
    Check(enet_socket_get_address(socket, &actual) == 0 && actual.port != 0, "Balanced ENet lifetime");
    enet_socket_destroy(socket); enet_deinitialize();
}
}
int main(int argc, char** argv) {
    try {
        Codec(); Cache();
        if (argc > 1 && std::string(argv[1]) == "--loopback") Loopback();
        std::cout << "Discovery: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Discovery: " << e.what() << '\n'; return 1;
    }
}
