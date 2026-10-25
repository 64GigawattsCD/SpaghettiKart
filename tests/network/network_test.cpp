#include "Session.h"
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
using namespace KartNet;
void check(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
void pump(const std::vector<Session*>& sessions, int ms = 100) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    do {
        for (auto s : sessions) s->Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < end);
}
void until(const std::vector<Session*>& sessions, const std::function<bool()>& predicate, const char* text) {
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!predicate() && std::chrono::steady_clock::now() < end) pump(sessions, 2);
    if (!predicate()) {
        for (auto s : sessions) std::cerr << "phase=" << PhaseName(s->phase) << " roster=" << s->roster.size()
            << " epoch=" << s->RaceEpoch() << " rejected=" << s->rejected << " error=" << s->error << '\n';
        check(false, text);
    }
}
Party party(int count, int offset = 0) {
    Party result;
    for (int i = 0; i < count; ++i) result.push_back({uint8_t(i), uint8_t((i + offset) % 8), 0, true});
    return result;
}
Message state(int count, uint32_t tick, bool finished = false) {
    Message m; m.tick = tick; m.time = 4.25f; m.karts.resize(count);
    for (int i = 0; i < count; ++i) {
        auto& k = m.karts[i]; k.participant = i + 1; k.rank = i;
        k.lap = finished ? 3 : 0; k.pos[0] = float(i * 100); k.tyres[11] = -215.25f;
    }
    return m;
}
uint16_t testPort() {
    check(enet_initialize() == 0, "Port probe initialization");
    const auto socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    ENetAddress address{};
    const bool ok = socket != ENET_SOCKET_NULL && enet_address_set_host_ip(&address, "127.0.0.1") == 0 &&
        enet_socket_bind(socket, &address) == 0 && enet_socket_get_address(socket, &address) == 0;
    if (socket != ENET_SOCKET_NULL) enet_socket_destroy(socket);
    enet_deinitialize();
    check(ok && address.port, "OS-selected loopback test port");
    return address.port;
}
void codecs() {
    Message m; m.content = 0x1234567890abcdef; m.character = 7;
    auto bytes = Encode(m); Message out;
    check(bytes[0] == 'S' && bytes[24] == 0x12, "Wire byte order");
    check(Decode(bytes.data(), bytes.size(), out) && out.content == m.content && out.party[0].character == 7, "Hello roundtrip");
    for (size_t n = 0; n < bytes.size(); ++n) check(!Decode(bytes.data(), n, out), "Truncation");
    bytes.push_back(0); check(!Decode(bytes.data(), bytes.size(), out), "Trailing byte");
    bytes = Encode(m); bytes[5]++; check(!Decode(bytes.data(), bytes.size(), out), "Version mismatch");
    m = state(4, 9); m.kind = Kind::Snapshot; m.session = 123; m.race = 2;
    m.karts[3].size = 0.7f;
    bytes = Encode(m);
    check(bytes.size() <= MaxPacket && bytes.size() <= 1200 && Decode(bytes.data(), bytes.size(), out),
          "Maximum four-kart chunk including size stays within 1200-byte budget");
    check(out.karts[3].pos[0] == 300 && out.karts[1].tyres[11] == -215.25f && out.race == 2, "Float endian and race epoch");
    check(out.karts[3].size == 0.7f, "Kart size 0.7 roundtrip");
    for (size_t n = 0; n < bytes.size(); ++n) {
        out.tick = 12345;
        check(!Decode(bytes.data(), n, out) && out.tick == 12345, "Snapshot truncation is transactional");
    }
    bytes[7] = 1; check(!Decode(bytes.data(), bytes.size(), out), "Reserved header byte");
    m.karts[1].tyres[11] = INFINITY; bytes = Encode(m);
    check(!Decode(bytes.data(), bytes.size(), out), "Invalid contact point");
    m = state(2, 1); m.kind = Kind::Snapshot; m.karts[0].speed = NAN; bytes = Encode(m);
    check(!Decode(bytes.data(), bytes.size(), out), "NaN");
    for (float size : {0.0f, -0.7f, 8.01f}) {
        m = state(4, 1); m.kind = Kind::Snapshot; m.karts[3].size = size; bytes = Encode(m);
        check(!Decode(bytes.data(), bytes.size(), out), "Kart size rejects zero, negative and above eight");
    }
    m = state(4, 1); m.kind = Kind::Snapshot; m.karts[3].size = 8.0f; bytes = Encode(m);
    check(Decode(bytes.data(), bytes.size(), out) && out.karts[3].size == 8.0f, "Kart size upper endpoint accepted");
    m = state(2, 1); m.kind = Kind::Snapshot; m.karts[1].participant = 1; bytes = Encode(m);
    check(!Decode(bytes.data(), bytes.size(), out), "Duplicate kart identity");
    m = state(8, 1); m.kind = Kind::Snapshot;
    bool budget = false;
    try { Encode(m); } catch (const std::length_error&) { budget = true; }
    check(budget, "Full eight-racer snapshots must be chunked");
    m = {}; m.kind = Kind::Input; m.input.participant = 8; m.input.x = -80; m.input.buttons = 0xa010;
    bytes = Encode(m); check(Decode(bytes.data(), bytes.size(), out) && out.input.x == -80, "Signed eighth-racer input");
    m.input.x = -81; bytes = Encode(m); check(!Decode(bytes.data(), bytes.size(), out), "Input range");
    m.input.x = 0; m.input.buttons = 0x1000; bytes = Encode(m);
    check(!Decode(bytes.data(), bytes.size(), out), "Local menu button does not travel as driving input");
    m.input.buttons = 0x37010800; bytes = Encode(m);
    check(Decode(bytes.data(), bytes.size(), out), "Manual shifting, clutch and directional item actions survive the wire");
    for (auto kind : {Kind::UpdateParty, Kind::Welcome, Kind::Lobby}) {
        for (uint32_t token : {0u, 0x01020304u, 0xffffffffu}) {
            m = {}; m.kind = kind; m.session = 123; m.connection = 2;
            m.revision = 0x12345678; m.partyRevision = token;
            m.party = party(2, 4); m.party[1].controller = 3;
            m.party[1].transmission = 1; m.party[1].ready = false;
            m.roster = {{1, 1, 0, 0, 0, 0, true, true}, {2, 2, 1, 0, 4, 1, false, true}};
            bytes = Encode(m);
            check(Decode(bytes.data(), bytes.size(), out) && out.kind == kind && out.partyRevision == token,
                  "Party request and per-peer Welcome/Lobby acknowledgment token roundtrip");
            if (kind == Kind::UpdateParty) {
                check(out.party.size() == 2 && out.party[0].character == 4 && out.party[1].controller == 3 &&
                      out.party[1].character == 5 && out.party[1].transmission == 1 && !out.party[1].ready,
                      "Party token preserves complete gapped physical-port update");
            } else {
                check(out.revision == m.revision && out.connection == 2 && out.roster.size() == 2 &&
                      out.roster[1].character == 4 && out.roster[1].transmission == 1,
                      "Party acknowledgment token remains distinct from lobby revision");
            }
            for (size_t n = 0; n < bytes.size(); ++n) {
                out.partyRevision = 0x87654321;
                check(!Decode(bytes.data(), n, out) && out.partyRevision == 0x87654321,
                      "Party request and acknowledgment truncation is transactional");
            }
        }
    }
    check(Newer(1, 0xffffffff) && !Newer(4, 5) && !Newer(4, 4), "Sequence wrap");
    std::mt19937 random(42);
    for (int i = 0; i < 20000; ++i) {
        std::vector<uint8_t> bad(random() % 1300);
        for (auto& b : bad) b = uint8_t(random());
        Decode(bad.data(), bad.size(), out);
    }
}
void rosterChecks() {
    check(!ValidParty({}) && !ValidParty(party(5)), "Party count bounds");
    auto duplicate = party(2); duplicate[1].controller = 0;
    check(!ValidParty(duplicate), "One physical controller per local participant");
    RaceSettings settings;
    check(settings.Valid(), "Default settings");
    settings.maxPlayers = 9; check(!settings.Valid(), "Eight-seat cap");
    settings = {}; settings.name.assign(48, 'A'); check(!settings.Valid(), "Bounded server name");
    std::vector<Identity> roster;
    for (int i = 0; i < 8; ++i) roster.push_back({uint16_t(i + 1), uint16_t(i < 4 ? 1 : 2), uint8_t(i), uint8_t(i % 4), uint8_t(i), 0, true, true});
    check(ValidRoster(roster), "Four plus four roster");
    for (int i = 0; i < 4; ++i) {
        check(EngineSlot(roster, 1, i + 1) == i && EngineSlot(roster, 2, i + 5) == i, "Local views stay first");
        check(EngineSlot(roster, 2, i + 1) == i + 4, "Remote racers do not create viewports");
    }
    roster[7].controller = 0; check(!ValidRoster(roster), "Duplicate connection-owned slot");
}
void localCup() {
    Session local; RaceSettings settings; settings.scope = 0;
    check(local.Local(123, party(4), settings) && local.LocalOnly() && local.LocalCount() == 4, "Four-local offline GP");
    for (int round = 0; round < 4; ++round) {
        check(local.CanStart() && local.Start() && local.RaceIndex() == round, "Cup advances exactly one course");
        local.Loaded(); check(local.phase == Phase::Racing, "Local load barrier");
        local.Publish(state(4, round + 1, true), true);
        check(local.phase == Phase::Finished && local.standings[0] == 9 * (round + 1), "Authoritative GP points");
        local.Publish(state(4, round + 2, true), true);
        check(local.standings[0] == 9 * (round + 1), "Finish awards once");
        auto coast = state(4, round + 3);
        coast.karts[0].rank = 3; coast.karts[0].pos[0] = 543.f;
        coast.standings.fill(999);
        local.PublishPresentation(coast);
        check(local.snapshot.tick == unsigned(round + 3) && local.snapshot.karts[0].pos[0] == 543.f,
              "Local results retain a moving background");
        check(local.snapshot.karts[0].rank == 0 && local.snapshot.karts[0].lap == 3 &&
              local.standings[0] == 9 * (round + 1) && local.snapshot.standings == local.standings,
              "Local background motion does not revise ranks, laps or points");
    }
    check(local.CupComplete() && !local.Start(), "Cup stops after four races");
    check(local.ReturnToLobby() && !local.CupComplete() && local.standings[0] == 0, "New cup resets session standings");
}
void soloDriverFlow() {
    for (int mode = 0; mode < 2; ++mode) {
        Session local; RaceSettings settings; settings.scope = 0; settings.mode = uint8_t(mode);
        auto driver = party(1); driver[0].ready = false;
        check(local.Local(123, driver, settings) && !local.CanStart(), "Solo setup waits for driver choices");
        driver[0].character = 6;
        driver[0].transmission = 1;
        check(local.UpdateParty(driver) && !local.CanStart(), "Solo character and transmission do not start early");
        local.SetPlayerReady(0, true);
        check(local.CanStart() && local.Start() && !local.Start(), "Solo confirmation starts exactly once");
        check(local.roster.size() == 1 && local.roster[0].character == 6 && local.roster[0].transmission == 1,
              "Solo load retains selected driver and transmission");
        local.Loaded(); check(local.phase == Phase::Racing, "Solo load has no remote barrier");
        local.Publish(state(1, 1, true), true);
        check(local.phase == Phase::Finished, "Solo results remain available");
        check(local.ReturnToLobby() && !local.CanStart(), "Solo setup return clears readiness");
        settings.cc = 2;
        check(local.SetSettings(settings) && !local.CanStart(), "Solo setup can be revised before a new selection");
        local.SetPlayerReady(0, true);
        check(local.Start(), "Solo revised setup starts without recreating session");
    }
}
void partyUpdateAcknowledgments(uint16_t port) {
    Session host, client; RaceSettings settings;
    auto joined = party(2, 4); joined[1].controller = 3;
    check(host.Host(port, 123, party(2), settings, {}, "127.0.0.1"), "Party acknowledgment host");
    check(client.Join("127.0.0.1", port, 123, joined), "Party acknowledgment gapped client joins");
    until({&host, &client}, [&] { return client.phase == Phase::Lobby && host.roster.size() == 4; },
          "Party acknowledgment admission");
    pump({&host, &client}, 25);
    check(!client.PartyUpdatePending() && !host.PartyUpdatePending(), "Welcome starts with no pending party update");
    const auto localCharacter = [&](const Session& session, int controller) {
        for (const auto& player : session.roster)
            if (player.connection == client.LocalConnection() && player.controller == controller) return int(player.character);
        return -1;
    };
    auto first = joined;
    first[0].character = 0; first[0].transmission = 1;
    first[1].character = 1;
    check(client.UpdateParty(first) && client.PartyUpdatePending(), "First party update immediately pending");
    // No host Poll: neither host processing nor an acknowledgment can occur.
    pump({&client}, 25);
    check(client.PartyUpdatePending() && localCharacter(host, 0) == 4 && localCharacter(client, 0) == 4,
          "Client polling without host leaves first request pending and acknowledged roster unchanged");
    until({&host}, [&] { return localCharacter(host, 0) == 2 && localCharacter(host, 1) == 3; },
          "Host normalizes first requested characters occupied by host drivers");
    check(client.PartyUpdatePending() && localCharacter(client, 0) == 4,
          "Host processing alone does not acknowledge on client");
    auto second = first;
    second[0].character = 6; second[0].transmission = 0;
    second[1].character = 0; second[1].transmission = 1;
    check(client.UpdateParty(second) && client.PartyUpdatePending(), "Second complete party update supersedes pending first");
    // The first ack is queued, but the host has not processed the second request.
    until({&client}, [&] { return localCharacter(client, 0) == 2 && localCharacter(client, 1) == 3; },
          "Client receives first normalized roster while second request remains unprocessed");
    check(client.PartyUpdatePending(), "First acknowledgment does not clear newer pending request");
    pump({&client}, 25);
    check(client.PartyUpdatePending() && localCharacter(host, 0) == 2,
          "Earlier acknowledgment stays pending until latest host update");
    until({&host, &client}, [&] { return !client.PartyUpdatePending(); }, "Latest acknowledgment clears pending request");
    check(host.phase == Phase::Lobby && client.phase == Phase::Lobby && ValidRoster(host.roster) && ValidRoster(client.roster),
          "Acknowledged normalized parties remain valid in lobby");
    check(host.roster.size() == 4 && client.roster.size() == 4 && client.LocalCount() == 2,
          "Complete gapped party update retains both local seats");
    check(localCharacter(host, 0) == 6 && localCharacter(host, 1) == 2 &&
          localCharacter(client, 0) == 6 && localCharacter(client, 1) == 2,
          "Latest acknowledgment carries host-normalized choices for every local driver");
    for (size_t i = 0; i < host.roster.size(); ++i) {
        const auto& expected = host.roster[i]; const auto& actual = client.roster[i];
        check(actual.participant == expected.participant && actual.connection == expected.connection &&
              actual.controller == expected.controller && actual.character == expected.character &&
              actual.transmission == expected.transmission && actual.ready == expected.ready,
              "Latest acknowledgment roster matches host identities, characters, transmission and readiness");
        if (actual.connection == client.LocalConnection())
            check(actual.transmission == second[actual.controller].transmission && !actual.ready,
                  "Latest complete party transmission choices survive host normalization and readiness reset");
    }
    host.SetReady(true); client.SetReady(true);
    until({&host, &client}, [&] {
        return host.CanStart() && std::all_of(client.roster.begin(), client.roster.end(), [](auto player) { return player.ready; });
    }, "Acknowledged party readies before authoritative load");
    const auto frozen = client.roster;
    auto late = second; late[0].character = 7; late[0].transmission = 1;
    check(client.UpdateParty(late) && client.PartyUpdatePending(), "Profile edit is pending before host freezes load");
    // Start without host Poll, so Load uses the previously acknowledged roster.
    check(host.Start() && host.phase == Phase::Loading, "Host freezes load before processing in-flight profile edit");
    until({&host, &client}, [&] { return client.phase == Phase::Loading; }, "Authoritative Load reaches pending client");
    check(!client.PartyUpdatePending(), "Authoritative Load cancels unacknowledged profile intent");
    pump({&host, &client}, 25);
    check(host.phase == Phase::Loading && client.phase == Phase::Loading && !client.PartyUpdatePending(),
          "Late party request does not reopen pending state or interrupt load");
    for (const auto* session : {&host, &client}) {
        check(session->roster.size() == frozen.size(), "Frozen load retains acknowledged party seats");
        for (size_t i = 0; i < frozen.size(); ++i)
            check(session->roster[i].character == frozen[i].character &&
                  session->roster[i].transmission == frozen[i].transmission,
                  "In-flight edit does not replace authoritative frozen profile choices");
    }
    host.Loaded(); client.Loaded();
    until({&host, &client}, [&] { return host.phase == Phase::Racing && client.phase == Phase::Racing; },
          "Cancelled in-flight profile intent does not stall the shared load barrier");
    client.Close();
    check(!client.PartyUpdatePending(), "Close clears party request tracking");
}
void eightRace(uint16_t port) {
    Session host, client, duplicate; RaceSettings settings;
    check(host.Host(port, 123, party(4), settings, "race-pass", "127.0.0.1"), "Host bind");
    check(!duplicate.Host(port, 123, 1, "127.0.0.1"), "Duplicate bind failure");
    check(client.Join("127.0.0.1", port, 123, party(4), "race-pass"), "Join whole party");
    until({&host,&client}, [&] { return client.phase == Phase::Lobby && host.roster.size() == 8; }, "Encrypted four plus four admission");
    check(host.LocalCount() == 4 && client.LocalCount() == 4 && client.LocalParticipant() == 5, "Connection ownership");
    check(ValidRoster(host.roster) && host.roster[7].character == 7, "Character collisions resolved for all seats");
    host.SetReady(false); client.SetReady(false); pump({&host,&client}, 25);
    for (int i = 0; i < 4; ++i) {
        host.SetPlayerReady(uint8_t(i), true); client.SetPlayerReady(uint8_t(i), true);
        pump({&host,&client}, 10);
        check(host.roster[i].ready && host.roster[i + 4].ready, "Each local driver can ready independently");
        if (i != 3) check(!host.CanStart(), "Every driver must be ready");
    }
    check(host.SetSettings(settings) && !host.CanStart(), "Rule changes invalidate old readiness");
    pump({&host,&client}, 25);
    for (auto p : client.roster) check(!p.ready, "Readiness reset reaches every system");
    host.SetReady(true); client.SetReady(true); pump({&host,&client}, 25);
    check(!client.Start() && host.Start(), "Only host starts");
    until({&host,&client}, [&] { return client.phase == Phase::Loading; }, "Load arrives");
    host.Loaded(); check(host.phase == Phase::Loading, "Host waits for every system");
    client.Loaded();
    until({&host,&client}, [&] { return client.phase == Phase::Racing && host.phase == Phase::Racing; }, "Shared start barrier");
    for (int i = 0; i < 4; ++i) {
        Input input; input.participant = i + 5; input.sequence = 1; input.throttle = 100 + i;
        client.Submit(input);
    }
    until({&host,&client}, [&] { return host.InputFor(8).sequence == 1; }, "Four independent input streams");
    for (int i = 0; i < 4; ++i) check(host.InputFor(i + 5).throttle == 100 + i, "Remote input stays on its owned seat");
    Input spoof; spoof.participant = 1; spoof.sequence = 99; spoof.throttle = 255; client.Submit(spoof);
    pump({&host,&client}, 20); check(host.InputFor(1).sequence == 0, "Client cannot submit another connection's input");
    auto snapshot = state(8, 1); host.Publish(snapshot);
    until({&host,&client}, [&] { return client.receivedSnapshots == 1; }, "Eight-kart chunks assemble atomically");
    check(client.snapshot.karts.size() == 8 && client.snapshot.karts[7].pos[0] == 700 && client.inputAck == 1, "Eight-racer snapshot and input acknowledgement");
    std::vector<uint8_t> world(1100, 42);
    host.PublishWorld(world);
    pump({&host,&client}, 30);
    auto updates = client.TakeWorld();
    check(updates.size() == 1 && updates.front() == world && client.TakeWorld().empty(), "Bounded host world updates drain once");
    client.PublishWorld(world); pump({&host,&client}, 20);
    check(host.TakeWorld().empty(), "Clients do not author world state");
    const auto originalParticipant = client.LocalParticipant();
    check(client.Reconnect(), "Reconnect retains a private resume ticket");
    until({&host,&client}, [&] { return client.phase == Phase::Racing && host.HasPeer(); }, "Encrypted party reconnect during race");
    check(client.LocalParticipant() == originalParticipant && client.LocalCount() == 4 && host.roster.size() == 8,
          "Reconnect preserves all four seats and local viewports");
    host.Publish(snapshot); pump({&host,&client}, 30); check(client.receivedSnapshots == 1, "Stale movement snapshot");
    pump({&host,&client}, 280); check(host.InputFor(8).throttle == 0, "Expired remote input becomes neutral");
    snapshot = state(8, 2, true); host.Publish(snapshot, true);
    until({&host,&client}, [&] { return client.phase == Phase::Finished; }, "Reliable final snapshot");
    check(client.standings == host.standings && client.snapshot.karts.size() == 8, "Matching standings on both systems");
    const auto finalPoints = host.standings;
    auto coast = state(8, 3);
    for (auto& kart : coast.karts) {
        kart.rank = 7; kart.lapTimes.fill(99999); kart.pos[0] += 321.f;
    }
    coast.standings.fill(999); coast.cupComplete = true;
    client.PublishPresentation(coast);
    check(client.snapshot.tick == 2, "Client cannot author post-race movement");
    host.PublishPresentation(coast);
    until({&host,&client}, [&] { return client.snapshot.tick == 3; }, "Post-race poses continue to every local viewport");
    check(client.phase == Phase::Finished && host.phase == Phase::Finished && client.snapshot.karts[7].pos[0] == 1021.f,
          "Post-race movement retains the finished phase");
    check(client.standings == finalPoints && host.standings == finalPoints && !client.CupComplete(),
          "Post-race movement cannot award points or complete another cup");
    for (int i = 0; i < 8; ++i)
        check(client.snapshot.karts[i].rank == i && client.snapshot.karts[i].lap == 3 &&
              client.snapshot.karts[i].lapTimes == snapshot.karts[i].lapTimes,
              "Post-race presentation preserves official finishing data");
    auto invalid = coast; invalid.tick = 4; invalid.karts[1].participant = invalid.karts[0].participant;
    host.PublishPresentation(invalid); host.PublishPresentation(coast);
    check(host.snapshot.tick == 3, "Duplicate participants and stale presentation ticks are ignored");
    auto postRaceWorld = std::vector<std::vector<uint8_t>>{{21, 22, 23}, {24, 25}};
    host.PublishWorldFrame(postRaceWorld);
    pump({&host,&client}, 30);
    check(client.TakeWorld() == postRaceWorld, "World movement continues behind results");
    const auto beforeReconnect = client.receivedSnapshots;
    check(client.Reconnect(), "Reconnect during animated results");
    until({&host,&client}, [&] { return client.phase == Phase::Finished && client.receivedSnapshots > beforeReconnect; },
          "Reconnect restores the latest post-race scene");
    pump({&host,&client}, 30);
    check(client.snapshot.tick == 3 && client.snapshot.karts[7].pos[0] == 1021.f && client.standings == finalPoints,
          "Reconnected scene retains final scoring and latest poses");
    check(client.TakeWorld() == postRaceWorld, "Reconnect restores latest post-race world chunks");
    check(host.Start(), "Next GP race");
    until({&host,&client}, [&] { return client.phase == Phase::Loading && client.RaceIndex() == 1; }, "Cup progression replicated");
    host.Loaded(); client.Loaded();
    until({&host,&client}, [&] { return client.phase == Phase::Racing; }, "Second start barrier");
    client.Close(); pump({&host,&client}, 100);
    check(host.phase == Phase::Racing && !host.roster[7].connected, "A lost client does not stop other racers");
}
void admission(uint16_t port) {
    Session host, client; RaceSettings settings; settings.maxPlayers = 6;
    check(host.Host(port, 123, party(4), settings, {}, "127.0.0.1"), "Capacity host");
    check(client.Join("127.0.0.1", port, 123, party(4)), "Oversized party attempts join");
    until({&host,&client}, [&] { return client.phase == Phase::Error || client.phase == Phase::Disconnected; }, "Party admission rejected");
    check(host.roster.size() == 4 && host.phase == Phase::Lobby, "Rejected party is never partially admitted");
    client.Close(); pump({&host,&client}, 20);
    check(client.Join("127.0.0.1", port, 999, party(1)), "Mismatch connect");
    until({&host,&client}, [&] { return client.phase == Phase::Error || client.phase == Phase::Disconnected; }, "Content mismatch rejected");
    check(host.phase == Phase::Lobby && !host.HasPeer(), "Bad client does not tear down the server");
    client.Close(); pump({&host,&client}, 20);
    check(client.Join("127.0.0.1", port, 123, party(2, 4)), "Smaller party joins");
    until({&host,&client}, [&] { return client.phase == Phase::Lobby && host.roster.size() == 6; }, "Capacity fits exact party");
    host.SetReady(false); check(!host.CanStart(), "Readiness gates start");
    host.SetReady(true); check(host.CanStart(), "Ready party can start");
    client.Close(); pump({&host,&client}, 100); check(host.roster.size() == 4, "Lobby leave releases all party seats");
}
void eightSystems(uint16_t port) {
    std::array<Session, 8> sessions; std::vector<Session*> pointers;
    for (auto& s : sessions) pointers.push_back(&s);
    check(sessions[0].Host(port, 123, 0, "127.0.0.1"), "Eight-system host");
    for (int i = 1; i < 8; ++i) check(sessions[i].Join("127.0.0.1", port, 123, uint8_t(i)), "Individual system join");
    until(pointers, [&] {
        for (auto& s : sessions) if (s.phase != Phase::Lobby || s.roster.size() != 8) return false;
        return true;
    }, "Eight connected systems share one roster");
    for (int i = 0; i < 8; ++i) check(sessions[i].LocalCount() == 1 && sessions[i].EngineSlot(sessions[i].LocalParticipant()) == 0,
                                     "Eight systems each keep one viewport");
    auto& host = sessions[0];
    check(host.Start(), "Eight-system race starts");
    until(pointers, [&] {
        for (auto& s : sessions) if (s.phase != Phase::Loading) return false;
        return true;
    }, "Every system receives the loading epoch");
    for (int i = 0; i < 7; ++i) sessions[i].Loaded();
    pump(pointers, 30);
    check(host.phase == Phase::Loading, "Eight-system barrier waits for last system");
    sessions[7].Loaded();
    until(pointers, [&] {
        for (auto& s : sessions) if (s.phase != Phase::Racing) return false;
        return true;
    }, "Eight systems enter race together");
    std::array<uint16_t, 8> participants{};
    for (int i = 0; i < 8; ++i) {
        participants[i] = sessions[i].LocalParticipant();
        Input input; input.participant = participants[i]; input.sequence = 1; input.throttle = uint8_t(101 + i);
        sessions[i].Submit(input);
    }
    until(pointers, [&] {
        for (auto id : participants) if (host.InputFor(id).sequence != 1) return false;
        return true;
    }, "Eight systems own eight input streams");
    for (int i = 0; i < 8; ++i) {
        check(host.InputFor(participants[i]).throttle == 101 + i, "Every system keeps its input identity");
        Input spoof; spoof.participant = participants[(i + 1) % 8]; spoof.sequence = 99; spoof.throttle = 255;
        sessions[i].Submit(spoof);
    }
    pump(pointers, 25);
    for (int i = 0; i < 8; ++i) check(host.InputFor(participants[i]).throttle == 101 + i,
                                     "Cross-system input spoof does not overwrite owned input");
    auto final = state(8, 1, true);
    host.Publish(final, true);
    until(pointers, [&] {
        for (auto& s : sessions) if (s.phase != Phase::Finished || s.snapshot.karts.size() != 8) return false;
        return true;
    }, "Eight-system final snapshot delivered to every client");
    const std::array<uint16_t, 8> expected{9, 6, 3, 1, 0, 0, 0, 0};
    for (auto& s : sessions) {
        check(s.standings == expected && s.standings == host.standings && s.RaceEpoch() == host.RaceEpoch(),
              "Eight systems agree on authoritative race points and epoch");
        for (int i = 0; i < 8; ++i)
            check(s.snapshot.karts[i].participant == i + 1 && s.snapshot.karts[i].rank == i && s.snapshot.karts[i].lap == 3,
                  "Eight-system finish preserves every rank and lap");
    }
}
void networkCup(uint16_t port) {
    Session host, client;
    RaceSettings settings;
    check(host.Host(port, 456, party(4), settings, {}, "127.0.0.1"), "Network cup host");
    check(client.Join("127.0.0.1", port, 456, party(4, 4)), "Network cup four-local guest");
    until({&host, &client}, [&] { return client.phase == Phase::Lobby && host.roster.size() == 8; }, "Network cup full roster");
    const auto first = client.LocalParticipant();
    const auto connection = client.LocalConnection();
    std::array<uint16_t, 8> expected{};
    constexpr uint16_t points[]{9, 6, 3, 1, 0, 0, 0, 0};
    const std::vector<std::vector<uint8_t>> finalWorld{
        {0, 1, 2, 255}, std::vector<uint8_t>(1100, 42), std::vector<uint8_t>(37, 173)};
    for (int round = 0; round < 4; ++round) {
        check(host.Start() && host.RaceIndex() == round, "Network cup advances one round");
        until({&host, &client}, [&] { return client.phase == Phase::Loading && client.RaceIndex() == round; },
              "Network cup round reaches client");
        check(host.RaceEpoch() == unsigned(round + 1) && client.RaceEpoch() == host.RaceEpoch(), "Fresh epoch each round");
        check(client.receivedSnapshots == 0, "New epoch discards preceding race snapshots");
        check(client.TakeWorld().empty(), "New epoch clears preceding world frame queue");
        if (round < 2) {
            if (round == 0) host.Loaded();
            else { client.Loaded(); pump({&host, &client}, 30); }
            check(client.Reconnect(), "Reconnect during loading");
            until({&host}, [&] { return !host.HasPeer(); }, "Loading interruption reaches host");
            check(host.roster.size() == 8 && !host.CanStart(), "Loading interruption retains all reserved seats");
            until({&host, &client}, [&] { return client.phase == Phase::Loading && host.HasPeer(); },
                  "Loading reconnect restores current epoch");
            check(client.LocalParticipant() == first && client.LocalConnection() == connection && client.LocalCount() == 4 &&
                  client.RaceEpoch() == host.RaceEpoch(), "Loading reconnect retains party and epoch");
            if (round == 0) {
                pump({&host, &client}, 25);
                check(host.phase == Phase::Loading, "Unloaded reconnect cannot bypass start barrier");
                client.Loaded();
            } else host.Loaded();
        } else { host.Loaded(); client.Loaded(); }
        until({&host, &client}, [&] { return host.phase == Phase::Racing && client.phase == Phase::Racing; },
              "Network cup start barrier including loading reconnect");
        Input input; input.participant = first; input.sequence = 1; input.throttle = uint8_t(120 + round);
        client.Submit(input);
        until({&host, &client}, [&] { return host.InputFor(first).throttle == 120 + round; }, "Input sequences restart each epoch");
        auto moving = state(8, 10);
        moving.time += float(round);
        host.Publish(moving);
        until({&host, &client}, [&] { return client.receivedSnapshots == 1; }, "Current epoch snapshot replaces preceding epoch");
        auto final = state(8, 11, true);
        final.time = 20.0f + float(round);
        for (int i = 0; i < 8; ++i) {
            final.karts[i].rank = uint8_t((i + round * 2) % 8);
            final.karts[i].pos[0] += float(1000 + round * 100);
            expected[i] += points[final.karts[i].rank];
        }
        if (round == 2) {
            check(client.Reconnect(), "Interrupt before final snapshot");
            until({&host}, [&] { return !host.HasPeer(); }, "Final interruption reaches host");
            host.PublishWorldFrame(finalWorld, true);
            host.Publish(final, true);
            check(client.snapshot.tick == 10, "Disconnected client has not received final state");
        } else host.Publish(final, true);
        until({&host, &client}, [&] {
            return client.phase == Phase::Finished && client.snapshot.tick == final.tick && client.snapshot.karts.size() == 8;
        }, "Final state recovers even when reconnect arrives after finish");
        if (round == 2) {
            std::vector<std::vector<uint8_t>> recovered;
            until({&host, &client}, [&] {
                auto chunks = client.TakeWorld();
                recovered.insert(recovered.end(), chunks.begin(), chunks.end());
                return recovered.size() >= finalWorld.size();
            }, "Finished reconnect resends every final world chunk");
            check(recovered == finalWorld, "Final world chunks retain order, boundaries and exact bytes");
            check(client.TakeWorld().empty(), "Recovered final world group drains once");
            const auto snapshots = client.receivedSnapshots;
            check(client.Reconnect(), "Replay final world group before next round");
            until({&host, &client}, [&] {
                return client.phase == Phase::Finished && client.receivedSnapshots > snapshots;
            }, "Second finished reconnect replays cached final state");
            // Leave the replay queued so the next Load must discard it.
            pump({&host, &client}, 30);
        } else if (round == 3) {
            const auto snapshots = client.receivedSnapshots;
            check(client.Reconnect(), "Reconnect after round without a final world group");
            until({&host, &client}, [&] {
                return client.phase == Phase::Finished && client.receivedSnapshots > snapshots;
            }, "Last-round final snapshot replays");
            pump({&host, &client}, 30);
            check(client.TakeWorld().empty(), "New round clears cached final world group on host");
        }
        check(client.LocalCount() == 4 && client.LocalParticipant() == first && client.LocalConnection() == connection,
              "Finished reconnect preserves whole local party");
        check(host.standings == expected && client.standings == expected, "All eight cumulative GP scores agree each round");
        check(client.CupComplete() == (round == 3) && host.CupComplete() == (round == 3), "Only fourth finish completes cup");
        check(client.snapshot.race == host.RaceEpoch() && client.snapshot.round == round && client.snapshot.time == final.time,
              "Recovered final state belongs to current epoch and round");
        for (int i = 0; i < 8; ++i) {
            const auto& kart = client.snapshot.karts[i];
            check(kart.participant == i + 1 && kart.rank == final.karts[i].rank && kart.lap == 3 && kart.pos[0] == final.karts[i].pos[0],
                  "Final ranks, lap counts and positions survive every GP round");
        }
        host.Publish(final, true); pump({&host, &client}, 10);
        check(host.standings == expected && client.standings == expected, "Repeated finish never awards twice");
    }
    check(!host.Start() && !client.Start(), "No fifth network GP round");
    check(host.ReturnToLobby(), "Host returns complete cup to lobby");
    until({&host, &client}, [&] { return client.phase == Phase::Lobby && client.standings == std::array<uint16_t, 8>{}; },
          "Cup points reset on both systems");
    check(!client.CupComplete() && !host.CupComplete() && !host.CanStart(), "New cup waits for fresh readiness");
}
void lobbyReconnect(uint16_t port) {
    Session host, client;
    check(host.Host(port, 789, party(4), RaceSettings{}, {}, "127.0.0.1"), "Lobby reconnect host");
    check(client.Join("127.0.0.1", port, 789, party(4, 4)), "Lobby reconnect whole party");
    until({&host, &client}, [&] { return client.phase == Phase::Lobby && host.roster.size() == 8; }, "Lobby reconnect admission");
    const auto original = client.roster;
    const auto connection = client.LocalConnection();
    check(client.Reconnect(), "Transient lobby interruption retains ticket");
    until({&host}, [&] { return !host.HasPeer(); }, "Lobby interruption observed before resume");
    check(host.roster.size() == 8 && !host.CanStart(), "Transient lobby party seats remain reserved");
    for (auto r : host.roster) if (r.connection == connection) check(!r.connected, "Reserved party marked disconnected");
    until({&host, &client}, [&] { return client.phase == Phase::Lobby && host.HasPeer(); }, "Transient lobby reconnect succeeds");
    check(client.LocalConnection() == connection && client.LocalCount() == 4 && client.roster.size() == 8,
          "Lobby reconnect restores same connection and four seats");
    for (size_t i = 0; i < original.size(); ++i) {
        const auto& r = client.roster[i]; const auto& old = original[i];
        check(r.participant == old.participant && r.connection == old.connection && r.controller == old.controller &&
              r.character == old.character && r.connected, "Lobby reconnect preserves participant and controller identity");
    }
    host.SetReady(true); client.SetReady(true);
    until({&host, &client}, [&] { return host.CanStart(); }, "Reconnected lobby can ready and start");
    client.Close();
    until({&host}, [&] { return host.roster.size() == 4 && !host.HasPeer(); }, "Explicit lobby leave releases rather than reserves seats");
}
int main() {
    try {
        codecs(); rosterChecks(); localCup(); soloDriverFlow(); partyUpdateAcknowledgments(testPort());
        eightRace(testPort()); admission(testPort()); eightSystems(testPort()); networkCup(testPort()); lobbyReconnect(testPort());
        std::cout << "Codec, party acknowledgments, malformed packets, local GP, full network GP, eight-system race, admission and reconnect checks passed\n";
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
