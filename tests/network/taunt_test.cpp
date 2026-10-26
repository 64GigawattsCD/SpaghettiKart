#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Protocol.h"
#include "SecureChannel.h"
#include <enet/enet.h>
#include <future>
#include <memory>
#include "Session.h"
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace KartNet;
namespace {
void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
void pump(const std::vector<Session*>& sessions, int ms = 30) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    do {
        for (auto s : sessions) s->Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < end);
}
void until(const std::vector<Session*>& sessions, const std::function<bool()>& predicate, const char* text) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!predicate() && std::chrono::steady_clock::now() < end) pump(sessions, 2);
    check(predicate(), text);
}
uint16_t port() {
    check(enet_initialize() == 0, "Port probe init");
    const auto socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    ENetAddress address{};
    const bool ok = socket != ENET_SOCKET_NULL && enet_address_set_host_ip(&address, "127.0.0.1") == 0 &&
        enet_socket_bind(socket, &address) == 0 && enet_socket_get_address(socket, &address) == 0;
    if (socket != ENET_SOCKET_NULL) enet_socket_destroy(socket);
    enet_deinitialize();
    check(ok && address.port, "Loopback ephemeral port");
    return address.port;
}
Party party(uint8_t character) { return {{1, character, 0, true}, {3, uint8_t(character + 1), 0, true}}; }
Message taunt(const Session& s, uint32_t sequence, uint16_t attacker = 1, uint16_t victim = 4, uint8_t character = 0) {
    Message m; m.kind = Kind::Taunt; m.session = s.SessionId(); m.race = s.RaceEpoch();
    m.taunt = {sequence, attacker, victim, character, 12.5f, 750}; return m;
}
Message finish(const Session& s) {
    Message m; m.tick = 900; m.time = 15;
    m.karts.clear();
    for (auto r : s.roster) { Kart k; k.participant = r.participant; k.rank = r.racer; m.karts.push_back(k); }
    return m;
}
void codecs() {
    Session unused;
    auto m = taunt(unused, 0x01020304, 8, 1, 7);
    m.session = 123; m.race = 99;
    auto bytes = Encode(m); Message out;
    check(bytes.size() == 37 && bytes[20] == 1 && bytes[23] == 4, "17-byte big-endian event payload");
    check(Decode(bytes.data(), bytes.size(), out) && out.kind == Kind::Taunt && out.session == 123 && out.race == 99 &&
          out.taunt.sequence == m.taunt.sequence && out.taunt.attacker == 8 && out.taunt.victim == 1 &&
          out.taunt.character == 7 && out.taunt.courseTime == 12.5f && out.taunt.tick == 750, "Taunt codec roundtrip");
    for (size_t n = 0; n < bytes.size(); ++n) {
        out.taunt.sequence = 42;
        check(!Decode(bytes.data(), n, out) && out.taunt.sequence == 42, "Transactional event truncation");
    }
    bytes.push_back(0); check(!Decode(bytes.data(), bytes.size(), out), "No arbitrary sound extension");
    bytes = Encode(m); bytes[5] = 4;
    check(!Decode(bytes.data(), bytes.size(), out), "Old application protocol rejected");
    const auto malformed = [&](TauntEvent e) {
        auto bad = m; bad.taunt = e; const auto wire = Encode(bad);
        check(!Decode(wire.data(), wire.size(), out), "Malformed taunt rejected");
    };
    auto e = m.taunt; e.sequence = 0; malformed(e);
    for (uint16_t id : {0, 9, 65535}) {
        e = m.taunt; e.attacker = id; malformed(e);
        e = m.taunt; e.victim = id; malformed(e);
    }
    e = m.taunt; e.victim = e.attacker; malformed(e);
    for (uint8_t character : {8, 255}) { e = m.taunt; e.character = character; malformed(e); }
    for (float time : {-1.0f, INFINITY, -INFINITY, NAN, 10000001.0f}) {
        e = m.taunt; e.courseTime = time; malformed(e);
    }
    for (float time : {0.0f, 10000000.0f}) {
        m.taunt.courseTime = time; bytes = Encode(m);
        check(Decode(bytes.data(), bytes.size(), out), "Course-time endpoints");
    }
}
}
namespace KartNet {
// White-box checks also exercise packets that normal client APIs never send.
struct SessionTestAccess {
static void localQueue() {
    Session s; RaceSettings settings;
    check(s.Local(123, party(0), settings), "Local radio session");
    check(!s.PublishTaunt(1, 2, 0, 1, 60), "No lobby taunt");
    check(s.Start() && !s.PublishTaunt(1, 2, 0, 1, 60), "No loading taunt");
    s.Loaded();
    check(!s.PublishTaunt(1, 1, 0, 1, 60) && !s.PublishTaunt(1, 8, 0, 1, 60) &&
          !s.PublishTaunt(8, 1, 7, 1, 60) && !s.PublishTaunt(1, 2, 1, 1, 60) &&
          !s.PublishTaunt(1, 2, 0, NAN, 60) && !s.PublishTaunt(1, 2, 0, -1, 60), "Invalid/CPU identities and time fail");
    for (uint32_t i = 0; i < MaxQueuedTaunts + 5; ++i)
        check(s.PublishTaunt(1, 2, 0, float(i), i), "Local queue accepts host event");
    auto events = s.TakeTaunts();
    check(events.size() == MaxQueuedTaunts && events.front().sequence == 6 && events.back().sequence == 37 &&
          events.front().tick == 5 && events.front().courseTime == 5 && s.TakeTaunts().empty(), "Bounded FIFO drops oldest and drains once");
    s.tauntSequence = std::numeric_limits<uint32_t>::max();
    check(s.PublishTaunt(2, 1, 1, 1, 1) && s.TakeTaunts().front().sequence == 1, "Author sequence wrap skips zero");
    check(s.PublishTaunt(1, 2, 0, 1, 1), "Queue before next race");
    s.Publish(finish(s), true);
    check(!s.PublishTaunt(1, 2, 0, 1, 1) && s.Start() && s.tauntQueue.empty() && s.tauntSequence == 0,
          "Start resets host queue and sequence");
    s.Loaded();
    check(s.PublishTaunt(1, 2, 0, 1, 1) && s.TakeTaunts().front().sequence == 1, "Next race starts at sequence one");
    s.PublishTaunt(1, 2, 0, 1, 1); s.Publish(finish(s), true);
    check(s.TakeTaunts().empty(), "No dispatch after racing");
    s.Close(); check(s.tauntQueue.empty() && s.tauntSequence == 0, "Close clears radio state");
}
static void loopback() {
    Session host, victim, observer; const auto testPort = port(); RaceSettings settings;
    check(host.Host(testPort, 123, party(0), settings, {}, "127.0.0.1") &&
          victim.Join("127.0.0.1", testPort, 123, party(4)), "Radio host and gapped-port victim join");
    until({&host, &victim}, [&] { return victim.phase == Phase::Lobby && host.roster.size() == 4; }, "Victim admission");
    check(observer.Join("127.0.0.1", testPort, 123, party(6)), "Observer join");
    std::vector<Session*> sessions{&host, &victim, &observer};
    until(sessions, [&] { return observer.phase == Phase::Lobby && host.roster.size() == 6; }, "Observer admission");
    pump(sessions);
    check(victim.LocalCount() == 2 && victim.EngineSlot(3) == 0 && victim.EngineSlot(4) == 1 &&
          victim.roster[2].controller == 0 && victim.roster[3].controller == 1,
          "Gapped physical ports retain normalized split-screen identity mapping");
    check(host.Start(), "Radio race start");
    until(sessions, [&] { return victim.phase == Phase::Loading && observer.phase == Phase::Loading; }, "Radio load");
    host.Loaded(); victim.Loaded(); observer.Loaded();
    until(sessions, [&] { return host.phase == Phase::Racing && victim.phase == Phase::Racing && observer.phase == Phase::Racing; }, "Radio begin");
    check(host.PublishTaunt(1, 4, 0, 12.5f, 750), "Host publishes only to victim connection");
    until(sessions, [&] { return victim.tauntQueue.size() == 1; }, "Reliable victim delivery");
    pump(sessions);
    check(host.TakeTaunts().empty() && observer.TakeTaunts().empty(), "Attacker and observer see no victim radio");
    auto events = victim.TakeTaunts();
    check(events.size() == 1 && events[0].sequence == 1 && events[0].attacker == 1 && events[0].victim == 4 &&
          events[0].character == 0 && events[0].tick == 750 && events[0].courseTime == 12.5f, "Exact victim payload");
    check(host.PublishTaunt(3, 2, 4, 13, 780), "Remote attacker targets host split-screen victim");
    events = host.TakeTaunts();
    check(events.size() == 1 && events[0].victim == 2 && host.EngineSlot(events[0].victim) == 1, "Host-local victim queues directly");
    pump(sessions); check(victim.TakeTaunts().empty() && observer.TakeTaunts().empty(), "Host-local event stays local");
    check(!victim.PublishTaunt(3, 2, 4, 13, 780), "Client API authorship rejected");
    auto forged = taunt(victim, 3);
    const auto rejected = host.rejected;
    check(victim.Send(victim.peers[0], forged), "Send authenticated client forgery");
    until(sessions, [&] { return host.rejected > rejected; }, "Host rejects encrypted client taunt");
    check(host.TakeTaunts().empty() && victim.TakeTaunts().empty() && observer.TakeTaunts().empty(), "No forged event relayed");

    auto& source = victim.peers[0];
    const auto rejectReceive = [&](Message m, uint8_t channel = 0) {
        const auto before = victim.rejected;
        victim.Receive(source, m, channel);
        check(victim.rejected == before + 1 && victim.TakeTaunts().empty(), "Invalid event receive rejected");
    };
    rejectReceive(taunt(victim, 1)); // Already dispatched sequence.
    rejectReceive(taunt(victim, 0));
    rejectReceive(taunt(victim, 3), 2);
    auto bad = taunt(victim, 3); --bad.race; rejectReceive(bad);
    bad = taunt(victim, 3); ++bad.session; rejectReceive(bad);
    rejectReceive(taunt(victim, 3, 1, 6)); // Another connection's victim.
    rejectReceive(taunt(victim, 3, 1, 4, 7));
    rejectReceive(taunt(victim, 3, 8, 4, 7)); // In-range CPU identity, not roster.
    rejectReceive(taunt(victim, 3, 1, 8));
    rejectReceive(taunt(victim, 3, 4, 4, 5));
    bad = taunt(victim, 3); bad.taunt.courseTime = NAN; rejectReceive(bad);
    source.admitted = false; rejectReceive(taunt(victim, 3)); source.admitted = true;
    source.connection = 2; rejectReceive(taunt(victim, 3)); source.connection = 1;
    victim.phase = Phase::Loading; rejectReceive(taunt(victim, 3)); victim.phase = Phase::Racing;
    victim.roster[0].connected = false; rejectReceive(taunt(victim, 3)); victim.roster[0].connected = true;
    victim.roster[3].connected = false; rejectReceive(taunt(victim, 3)); victim.roster[3].connected = true;
    victim.Receive(source, taunt(victim, 3), 0);
    check(victim.TakeTaunts().size() == 1 && victim.receivedTauntSequence == 3, "Invalid messages do not consume sequence");
    rejectReceive(taunt(victim, 2)); rejectReceive(taunt(victim, 3));
    victim.receivedTauntSequence = 0xfffffffe;
    victim.Receive(source, taunt(victim, 0xffffffff), 0);
    victim.Receive(source, taunt(victim, 1), 0);
    check(victim.TakeTaunts().size() == 2 && victim.receivedTauntSequence == 1, "Receive sequence wrap");
    victim.receivedTauntSequence = 0;
    for (uint32_t i = 1; i <= MaxQueuedTaunts + 5; ++i) victim.Receive(source, taunt(victim, i), 0);
    events = victim.TakeTaunts();
    check(events.size() == MaxQueuedTaunts && events.front().sequence == 6, "Remote receive queue bounded");
    victim.receivedTauntSequence = 0;

    check(host.PublishTaunt(1, 4, 0, 14, 840), "Queue pre-disconnect event");
    until(sessions, [&] { return !victim.tauntQueue.empty(); }, "Pre-disconnect event received but not dispatched");
    const auto lastSequence = victim.receivedTauntSequence;
    auto hostPeer = std::find_if(host.peers.begin(), host.peers.end(), [&](const auto& p) {
        return p.admitted && p.connection == victim.LocalConnection();
    });
    check(hostPeer != host.peers.end(), "Victim transport found");
    check(host.PublishTaunt(1, 4, 0, 14, 841), "Old transport has pending reliable event");
    enet_peer_disconnect_now(hostPeer->transport, 0); host.Remove(*hostPeer);
    enet_peer_disconnect_now(source.transport, 0); victim.Remove(source);
    check(victim.TakeTaunts().empty() && !host.PublishTaunt(1, 4, 0, 14, 842), "Disconnect clears queue and rejects disconnected target");
    check(victim.Reconnect() && victim.TakeTaunts().empty(), "Reconnect begins with empty queue");
    until(sessions, [&] { return victim.phase == Phase::Racing && host.HasPeer() && host.roster[3].connected; }, "Same-race reconnect");
    pump(sessions, 100);
    check(victim.TakeTaunts().empty() && victim.receivedTauntSequence == lastSequence, "No old queued or in-flight radio replay on reconnect");
    rejectReceive(taunt(victim, lastSequence));
    check(host.PublishTaunt(1, 4, 0, 14.5f, 870), "Fresh post-reconnect event");
    until(sessions, [&] { return !victim.tauntQueue.empty(); }, "Fresh event delivered");
    check(victim.TakeTaunts().size() == 1, "Only new radio after reconnect");

    check(host.PublishTaunt(1, 4, 0, 15, 900), "Pending event before race change");
    until(sessions, [&] { return !victim.tauntQueue.empty(); }, "Pending event queued");
    const auto old = taunt(victim, 999);
    host.Publish(finish(host), true);
    until(sessions, [&] { return victim.phase == Phase::Finished && observer.phase == Phase::Finished; }, "Finish radio race");
    check(host.Start(), "Second radio race");
    until(sessions, [&] { return victim.phase == Phase::Loading && observer.phase == Phase::Loading; }, "Second radio load");
    check(victim.tauntQueue.empty() && victim.receivedTauntSequence == 0 && host.tauntSequence == 0, "Load clears queue and sequence");
    host.Loaded(); victim.Loaded(); observer.Loaded();
    until(sessions, [&] { return host.phase == Phase::Racing && victim.phase == Phase::Racing && observer.phase == Phase::Racing; }, "Second radio begin");
    rejectReceive(old);
    check(host.PublishTaunt(1, 4, 0, 0, 1), "New race event");
    until(sessions, [&] { return !victim.tauntQueue.empty(); }, "New race victim delivery");
    events = victim.TakeTaunts();
    check(events.size() == 1 && events.front().sequence == 1, "New epoch accepts reset sequence");
    check(host.TakeTaunts().empty() && observer.TakeTaunts().empty(), "Victim visibility persists across epochs");
}
};
}
int main() {
    try {
        codecs(); SessionTestAccess::localQueue(); SessionTestAccess::loopback();
        std::cout << "Victim radio codecs, authority, routing, queues, epochs and reconnect passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
