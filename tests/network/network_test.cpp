#include "Session.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
using namespace KartNet;
void check(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
void pump(Session& a, Session& b, int ms = 100) {
    auto end = std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now()<end) { a.Poll(); b.Poll(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
}
void codecs() {
    Message m; m.content = 0x1234567890abcdef; m.character = 7;
    auto bytes = Encode(m); Message out;
    check(bytes.size() == 29 && bytes[0] == 'S' && bytes[20] == 0x12, "Wire byte order");
    check(Decode(bytes.data(), bytes.size(), out) && out.content == m.content, "Hello roundtrip");
    for (size_t n = 0; n < bytes.size(); ++n) check(!Decode(bytes.data(), n, out), "Truncation");
    bytes.push_back(0); check(!Decode(bytes.data(), bytes.size(), out), "Trailing byte");
    bytes = Encode(m); bytes[5]++; check(!Decode(bytes.data(), bytes.size(), out), "Version mismatch");
    m.kind = Kind::Snapshot; m.session = 123; m.tick = 9; m.time = 4.25f;
    m.karts[0].participant = 1; m.karts[1].participant = 2; m.karts[1].pos[0] = -125.5f;
    m.karts[1].tyres[11] = -215.25f;
    bytes = Encode(m); check(bytes.size() < MaxPacket && Decode(bytes.data(), bytes.size(), out), "Snapshot roundtrip");
    check(out.karts[1].pos[0] == -125.5f, "Float endian");
    check(out.karts[1].tyres[11] == -215.25f, "Cockpit contact points");
    for (size_t n = 0; n < bytes.size(); ++n) {
        out.tick = 12345;
        check(!Decode(bytes.data(), n, out) && out.tick == 12345, "Snapshot truncation is transactional");
    }
    bytes[7] = 1; check(!Decode(bytes.data(), bytes.size(), out), "Reserved header byte");
    m.karts[1].tyres[11] = INFINITY; bytes = Encode(m);
    check(!Decode(bytes.data(), bytes.size(), out), "Invalid contact point");
    m.karts[1].tyres[11] = 0;
    m.karts[0].speed = NAN; bytes = Encode(m); check(!Decode(bytes.data(), bytes.size(), out), "NaN");
    m = {}; m.kind = Kind::Input; m.input.participant = 2; m.input.x = -80; m.input.buttons = 0x8010;
    bytes = Encode(m); check(Decode(bytes.data(), bytes.size(), out) && out.input.x == -80, "Signed input");
    m.input.x = -81; bytes = Encode(m); check(!Decode(bytes.data(), bytes.size(), out), "Input range");
    m.input.x = 0; m.input.buttons = 0x2000; bytes = Encode(m); check(!Decode(bytes.data(), bytes.size(), out), "Item not in protocol v1");
    check(Newer(1, 0xffffffff) && !Newer(4, 5) && !Newer(4, 4), "Sequence wrap");
    check(LocalSlot(2, 2) == 0 && LocalSlot(1, 2) == 1 && LocalSlot(3, 2) == -1, "Participant mapping");
    std::mt19937 random(42);
    for (int i = 0; i < 20000; ++i) {
        std::vector<uint8_t> bad(random()%1300); for (auto& b : bad) b = uint8_t(random());
        Decode(bad.data(), bad.size(), out);
    }
}
void loopback(uint16_t port) {
    Session host, client, duplicate;
    check(host.Host(port, 123, 0, "127.0.0.1"), "Host bind");
    check(!duplicate.Host(port, 123, 1, "127.0.0.1"), "Duplicate bind failure");
    check(client.Join("127.0.0.1", port, 123, 0), "Join"); pump(host, client);
    check(host.HasPeer() && client.phase == Phase::Lobby, "Admission");
    check(client.roster[1].character == 1 && client.LocalParticipant() == 2, "Roster ownership");
    check(!client.Start(), "Client start rejected");
    check(host.Start(), "Host starts"); pump(host, client);
    host.Loaded(); check(host.phase == Phase::Loading, "Load barrier");
    client.Loaded(); pump(host, client); check(client.phase == Phase::Racing && host.phase == Phase::Racing, "Start barrier");
    Input i; i.sequence = 1; i.buttons = 0x8000; i.throttle = 255; client.Submit(i); pump(host, client, 40);
    check(host.RemoteInput().sequence == 1 && host.RemoteInput().throttle == 255, "Remote input");
    client.Submit(i); pump(host, client, 30); check(host.rejected > 0, "Duplicate input");
    pump(host, client, 280); check(host.RemoteInput().buttons == 0, "Expired input neutral");
    Message m; m.tick = 1; m.karts[0].participant = 1; m.karts[1].participant = 2; m.karts[1].pos[2] = 100;
    host.inputAck = 1; host.Publish(m); pump(host, client);
    check(client.receivedSnapshots == 1 && client.snapshot.karts[1].pos[2] == 100 && client.inputAck == 1, "Snapshot and ack");
    host.Publish(m); pump(host, client); check(client.receivedSnapshots == 1, "Stale snapshot");
    m.tick = 2; m.karts[1].lap = 3; host.Publish(m, true); pump(host, client);
    check(client.phase == Phase::Finished && client.snapshot.karts[1].lap == 3, "Reliable finish");
    client.Close(); pump(host, client); check(host.phase == Phase::Disconnected, "Disconnect");
    host.Close(); check(host.Host(port, 123, 0, "127.0.0.1"), "Restart bind");
    check(client.Join("127.0.0.1", port, 999, 1), "Mismatch connect"); pump(host, client);
    check(host.phase == Phase::Error && !host.HasPeer(), "Content mismatch");
}
int main() {
    try { codecs(); loopback(uint16_t(55000 + std::random_device{}()%5000)); std::cout << "Codec, 20k malformed packets, loopback race lifecycle passed\n"; }
    catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
