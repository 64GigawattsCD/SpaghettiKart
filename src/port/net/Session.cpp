#include "Session.h"
#include <chrono>
#include <random>

namespace KartNet {
const char* PhaseName(Phase p) {
    switch (p) {
    case Phase::Offline: return "Offline"; case Phase::Connecting: return "Connecting";
    case Phase::Lobby: return "Lobby"; case Phase::Loading: return "Loading";
    case Phase::Racing: return "Racing"; case Phase::Finished: return "Finished";
    case Phase::Disconnected: return "Disconnected"; default: return "Error";
    }
}
Session::~Session() { Close(); }
void Session::Close() {
    if (peer) enet_peer_disconnect_now(peer, 0);
    if (host) enet_host_destroy(host);
    if (transportInitialized) enet_deinitialize();
    transportInitialized = false;
    host = nullptr; peer = nullptr; phase = Phase::Offline; admitted = false;
    localLoaded = remoteLoaded = false; remoteInput = {}; snapshot = {};
    receivedSnapshots = rejected = sent = received = inputAck = 0; sessionId = 0; error.clear();
}
void Session::Fail(const char* text) {
    if (peer) enet_peer_disconnect_now(peer, 1);
    peer = nullptr; admitted = false; phase = Phase::Error; error = text; remoteInput = {};
}
bool Session::Open(bool server, uint16_t port, uint64_t content, uint8_t character, const char* bindAddress) {
    Close(); authority = server; contentHash = content; chosenCharacter = character;
    if (!port || character > 7 || enet_initialize() != 0) { Fail("Network initialization failed"); return false; }
    transportInitialized = true;
    ENetAddress addr{ENET_HOST_ANY, port};
    if (bindAddress && enet_address_set_host_ip(&addr, bindAddress) != 0) { Fail("Invalid bind IPv4"); return false; }
    host = enet_host_create(server ? &addr : nullptr, server ? 2 : 1, 3, 0, 0);
    if (!host) { Fail("Network port is occupied or unavailable"); return false; }
    host->maximumPacketSize = MaxPacket; host->maximumWaitingData = 65536;
    phase = server ? Phase::Lobby : Phase::Connecting; phaseAt = enet_time_get();
    if (server) {
        std::random_device random;
        sessionId = (uint64_t(random()) << 32) ^ random(); if (!sessionId) sessionId = 1;
        roster = {{{1, 1, 0, 0, character}, {2, 2, 1, 0, uint8_t((character+1)%8)}}};
    }
    return true;
}
bool Session::Host(uint16_t port, uint64_t content, uint8_t character, const char* bindAddress) {
    return Open(true, port, content, character, bindAddress);
}
bool Session::Join(const std::string& address, uint16_t port, uint64_t content, uint8_t character) {
    if (!Open(false, port, content, character)) return false;
    ENetAddress addr{}; addr.port = port;
    if (enet_address_set_host_ip(&addr, address.c_str()) != 0) { Fail("Enter an IPv4 address"); return false; }
    peer = enet_host_connect(host, &addr, 3, ProtocolVersion);
    if (!peer) { Fail("Connection allocation failed"); return false; }
    return true;
}
void Session::Send(Message m, bool reliable) {
    if (!peer) return;
    m.session = m.kind == Kind::Hello ? 0 : sessionId;
    auto bytes = Encode(m);
    uint8_t channel = m.kind == Kind::Input ? 1 : (m.kind == Kind::Snapshot ? 2 : 0);
    auto packet = enet_packet_create(bytes.data(), bytes.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (!packet) { Fail("Packet allocation failed"); return; }
    if (enet_peer_send(peer, channel, packet) != 0) { enet_packet_destroy(packet); Fail("Packet queue failed"); return; }
    ++sent;
    enet_host_flush(host);
}
void Session::Poll() {
    if (!host) return;
    ENetEvent event{};
    for (int n = 0; n < 64 && enet_host_service(host, &event, 0) > 0; ++n) {
        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            if (authority) {
                if (peer || phase != Phase::Lobby || event.data != ProtocolVersion) {
                    enet_peer_disconnect_now(event.peer, 2); continue;
                }
                peer = event.peer;
            }
            enet_peer_timeout(event.peer, 8, 2000, 5000); enet_peer_ping_interval(event.peer, 250);
            phaseAt = enet_time_get();
            if (!authority) { Message m; m.kind = Kind::Hello; m.content = contentHash; m.character = chosenCharacter; Send(m); }
        } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            ++received; Message m;
            if (event.peer == peer && Decode(event.packet->data, event.packet->dataLength, m)) Receive(m, event.channelID);
            else ++rejected;
            enet_packet_destroy(event.packet);
        } else if (event.type == ENET_EVENT_TYPE_DISCONNECT && event.peer == peer) {
            peer = nullptr; admitted = false; remoteInput = {}; phase = Phase::Disconnected;
            error = "Peer disconnected; race stopped";
        }
    }
    auto age = enet_time_get() - phaseAt;
    if ((phase == Phase::Connecting && age > 8000) || (phase == Phase::Loading && age > 30000) ||
        (authority && peer && !admitted && phase == Phase::Lobby && age > 8000)) Fail("Peer timed out");
}
void Session::Receive(const Message& m, uint8_t channel) {
    uint8_t expected = m.kind == Kind::Input ? 1 : (m.kind == Kind::Snapshot ? 2 : 0);
    if (channel != expected) { ++rejected; return; }
    if (authority && m.kind == Kind::Hello && phase == Phase::Lobby && !admitted) {
        if (m.rules != RulesVersion || m.content != contentHash) { Fail("Game content or race rules differ"); return; }
        roster[1].character = m.character == roster[0].character ? (m.character+1)%8 : m.character;
        admitted = true;
        Message welcome; welcome.kind = Kind::Welcome; welcome.content = contentHash; welcome.roster = roster; Send(welcome);
        return;
    }
    if (!authority && m.kind == Kind::Welcome && phase == Phase::Connecting) {
        if (m.rules != RulesVersion || m.content != contentHash) { Fail("Game content or race rules differ"); return; }
        sessionId = m.session; roster = m.roster; admitted = true; phase = Phase::Lobby; return;
    }
    if (!admitted || !sessionId || m.session != sessionId) { ++rejected; return; }
    if (!authority && m.kind == Kind::Load && phase == Phase::Lobby) {
        phase = Phase::Loading; phaseAt = enet_time_get(); return;
    }
    if (authority && m.kind == Kind::Loaded && phase == Phase::Loading) { remoteLoaded = true; TryBegin(); return; }
    if (!authority && m.kind == Kind::Begin && phase == Phase::Loading && localLoaded) {
        phase = Phase::Racing; return;
    }
    if (authority && m.kind == Kind::Input && phase == Phase::Racing && m.input.participant == 2 &&
        Newer(m.input.sequence, remoteInput.sequence)) {
        remoteInput = m.input; lastInputAt = enet_time_get(); return;
    }
    if (!authority && (m.kind == Kind::Snapshot || m.kind == Kind::Finish) &&
        (phase == Phase::Racing || phase == Phase::Finished) &&
        (!receivedSnapshots || Newer(m.tick, snapshot.tick) || (m.kind == Kind::Finish && m.tick == snapshot.tick))) {
        snapshot = m; ++receivedSnapshots; lastSnapshotAt = enet_time_get(); inputAck = m.acknowledgedInput;
        if (m.kind == Kind::Finish) phase = Phase::Finished;
        return;
    }
    ++rejected;
}
bool Session::Start() {
    if (!authority || !admitted || phase != Phase::Lobby) return false;
    Message m; m.kind = Kind::Load; Send(m); phase = Phase::Loading; phaseAt = enet_time_get(); return true;
}
void Session::Loaded() {
    if (phase != Phase::Loading || localLoaded) return;
    localLoaded = true;
    if (!authority) { Message m; m.kind = Kind::Loaded; Send(m); } else TryBegin();
}
void Session::TryBegin() {
    if (!localLoaded || !remoteLoaded) return;
    Message m; m.kind = Kind::Begin; Send(m); phase = Phase::Racing;
}
void Session::Submit(Input i) {
    if (authority || phase != Phase::Racing) return;
    i.participant = LocalParticipant(); Message m; m.kind = Kind::Input; m.input = i; Send(m, false);
}
Input Session::RemoteInput() const {
    if (phase != Phase::Racing || enet_time_get() - lastInputAt > 250) return {};
    return remoteInput;
}
uint32_t Session::SnapshotAge() const { return receivedSnapshots ? enet_time_get() - lastSnapshotAt : 0; }
void Session::Publish(Message m, bool finish) {
    if (!authority || phase != Phase::Racing) return;
    m.kind = finish ? Kind::Finish : Kind::Snapshot; m.acknowledgedInput = inputAck;
    snapshot = m; Send(m, finish);
    if (finish) phase = Phase::Finished;
}
}
