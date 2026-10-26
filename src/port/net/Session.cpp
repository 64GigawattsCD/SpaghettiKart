#include "Session.h"
#include <algorithm>
#include <cstring>
#include <random>
#include <chrono>
#include <monocypher.h>

namespace KartNet {
namespace {
constexpr size_t WireLimit = MaxPacket + 96;
uint8_t Channel(Kind k) { return k == Kind::Input ? 1 : (k == Kind::Snapshot || k == Kind::World) ? 2 : 0; }
bool RawSend(ENetPeer* peer, const std::vector<uint8_t>& bytes, uint8_t channel, bool reliable = true) {
    auto packet = enet_packet_create(bytes.data(), bytes.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (!packet) return false;
    if (enet_peer_send(peer, channel, packet) == 0) return true;
    enet_packet_destroy(packet); return false;
}
std::vector<uint8_t> KeyPacket(uint8_t kind, const SecureChannel& crypto) {
    std::vector<uint8_t> data{'S','K','K',3,kind};
    data.insert(data.end(), crypto.PublicKey().begin(), crypto.PublicKey().end());
    if (kind == 1) data.insert(data.end(), crypto.Salt().begin(), crypto.Salt().end());
    return data;
}
}
const char* PhaseName(Phase p) {
    switch (p) {
    case Phase::Offline: return "Offline"; case Phase::Connecting: return "Connecting"; case Phase::Lobby: return "Lobby";
    case Phase::Loading: return "Loading"; case Phase::Racing: return "Racing"; case Phase::Finished: return "Finished";
    case Phase::Disconnected: return "Disconnected"; default: return "Error";
    }
}
Session::~Session() { Close(); }
void Session::ResetPeer(Peer& p) {
    if (p.exchange.valid()) p.exchange.wait();
    p = Peer{};
}
void Session::RetirePeer(Peer& p) {
    if (p.exchange.valid() && p.exchange.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        for (auto& retired : retiredExchanges) if (!retired.exchange.valid()) {
            retired = std::move(p); retired.transport = nullptr; p = Peer{}; return;
        }
    }
    ResetPeer(p);
}
bool Session::AllowHandshake(uint32_t address) {
    const auto now = enet_time_get();
    if (now - handshakeSince >= 10000) { handshakeSince = now; handshakeAttempts = 0; }
    if (handshakeAttempts >= 16) return false;
    auto found = std::find_if(handshakeRates.begin(), handshakeRates.end(), [address](auto r) {
        return r.count && r.address == address;
    });
    if (found == handshakeRates.end()) {
        found = std::min_element(handshakeRates.begin(), handshakeRates.end(), [now](auto a, auto b) {
            if (!a.count || !b.count) return !a.count && b.count;
            return now - a.since > now - b.since;
        });
        *found = {address, now, 0};
    }
    if (now - found->since >= 10000) { found->since = now; found->count = 0; }
    if (found->count >= 8) return false;
    ++found->count; ++handshakeAttempts; return true;
}
void Session::PollHandshakes() {
    for (auto& p : retiredExchanges)
        if (p.exchange.valid() && p.exchange.wait_for(std::chrono::seconds(0)) == std::future_status::ready) ResetPeer(p);
    for (auto& p : peers) {
        if (!p.exchange.valid() || p.exchange.wait_for(std::chrono::seconds(0)) != std::future_status::ready) continue;
        bool established = false;
        try { established = p.exchange.get(); } catch (const std::exception&) {}
        if (!established) {
            if (p.transport) enet_peer_disconnect_now(p.transport, 2);
            Remove(p);
            if (!authority) Fail("Connection key exchange failed");
            continue;
        }
        if (!authority) {
            if (!RawSend(p.transport, KeyPacket(2, *p.crypto), 0)) { Fail("Key reply queue failed"); continue; }
            Message hello; hello.kind = Kind::Hello; hello.content = contentHash;
            hello.party = localParty; hello.ticket = resumeTicket;
            Send(p, hello);
        } else if (!p.pendingHello.empty()) {
            auto pending = std::move(p.pendingHello); p.pendingHello.clear();
            ReceivePacket(p, pending.data(), pending.size(), 0);
        }
    }
    unsigned running = 0;
    for (const auto& p : peers) if (p.exchange.valid()) ++running;
    for (const auto& p : retiredExchanges) if (p.exchange.valid()) ++running;
    for (auto& p : peers) {
        if (!p.keyPending || running >= 2) continue;
        p.keyPending = false;
        auto crypto = p.crypto.get(); auto key = p.remoteKey; auto salt = p.remoteSalt;
        try {
            p.exchange = std::async(std::launch::async, [crypto, key, salt] { return crypto->Establish(key, salt); });
            ++running;
        } catch (const std::exception&) {
            if (p.transport) enet_peer_disconnect_now(p.transport, 2);
            Remove(p);
            if (!authority) Fail("Connection worker allocation failed");
        }
    }
}
void Session::Close() {
    for (auto& p : peers) {
        if (p.transport) enet_peer_disconnect_now(p.transport, 4);
        ResetPeer(p);
    }
    for (auto& p : retiredExchanges) ResetPeer(p);
    if (host) enet_host_destroy(host);
    host = nullptr;
    if (transportInitialized) enet_deinitialize();
    transportInitialized = false; localOnly = authority = localLoaded = cupComplete = false;
    phase = Phase::Offline; roster.clear(); inputs = {}; inputAt = {}; snapshot = {}; standings = {};
    assembling = {}; assembled = 0; worldQueue.clear(); finalWorld.clear(); localParty.clear(); connectionId = 0; sessionId = 0; raceId = revision = 0;
    receivedSnapshots = rejected = sent = received = inputAck = lastSnapshotAt = 0; error.clear();
    if (!password.empty()) crypto_wipe(password.data(), password.size());
    password.clear();
    crypto_wipe(resumeTicket.data(), resumeTicket.size());
    crypto_wipe(tickets.data(), sizeof(tickets));
    remoteAddress.clear(); remotePort = 0; resumeTicket = {}; tickets = {}; reconnectAt = disconnectedAt = 0;
    handshakeRates = {}; handshakeSince = handshakeAttempts = 0;
    partySent = partyAck = 0;
    reservationAt = {};
    tauntQueue.clear(); tauntSequence = receivedTauntSequence = 0;
}
void Session::Fail(const char* text) {
    error = text; phase = Phase::Error; inputs = {};
    tauntQueue.clear();
    for (auto& p : peers) if (p.transport) enet_peer_disconnect_later(p.transport, 1);
}
bool Session::Open(bool server, uint16_t port, uint64_t content, const Party& party,
                   const std::string& secret, const char* bindAddress) {
    Close(); authority = server; connectionId = server ? 1 : 0; contentHash = content; localParty = party; password = secret;
    if (!port || !ValidParty(party) || secret.size() > 64 || enet_initialize() != 0) {
        Fail("Network initialization failed"); return false;
    }
    transportInitialized = true;
    ENetAddress addr{ENET_HOST_ANY, port};
    if (bindAddress && enet_address_set_host_ip(&addr, bindAddress) != 0) { Fail("Invalid bind IPv4"); return false; }
    host = enet_host_create(server ? &addr : nullptr, server ? 16 : 1, 3, 0, 0);
    if (!host) { Fail("Network port is occupied or unavailable"); return false; }
    host->maximumPacketSize = WireLimit; host->maximumWaitingData = 256 * 1024;
    phase = server ? Phase::Lobby : Phase::Connecting; phaseAt = enet_time_get();
    if (server) {
        std::random_device random;
        sessionId = (uint64_t(random()) << 32) ^ random(); if (!sessionId) sessionId = 1;
    }
    return true;
}
bool Session::Host(uint16_t port, uint64_t content, uint8_t character, const char* bindAddress) {
    return Host(port, content, Party{{0, character, 0, true}}, RaceSettings{}, {}, bindAddress);
}
bool Session::Join(const std::string& address, uint16_t port, uint64_t content, uint8_t character) {
    return Join(address, port, content, Party{{0, character, 0, true}});
}
bool Session::Host(uint16_t port, uint64_t content, const Party& party, const RaceSettings& config,
                   const std::string& secret, const char* bindAddress) {
    if (!config.Valid() || party.size() > config.maxPlayers) { Close(); Fail("Invalid server settings"); return false; }
    if (!Open(true, port, content, party, secret, bindAddress)) return false;
    settings = config; settings.round = 0;
    if (!Admit(1, party)) { Close(); Fail("Local party does not fit"); return false; }
    return true;
}
bool Session::Local(uint64_t content, const Party& party, const RaceSettings& config) {
    Close();
    if (!config.Valid() || !ValidParty(party) || party.size() > config.maxPlayers) { Fail("Invalid local race settings"); return false; }
    authority = localOnly = true; connectionId = 1; contentHash = content; settings = config; settings.scope = 0;
    settings.round = 0; localParty = party; sessionId = 1; phase = Phase::Lobby;
    Admit(1, party); return true;
}
bool Session::Join(const std::string& address, uint16_t port, uint64_t content, const Party& party, const std::string& secret) {
    if (!Open(false, port, content, party, secret, nullptr)) return false;
    remoteAddress = address; remotePort = port;
    ENetAddress addr{}; addr.port = port;
    if (address.empty() || address.size() > 253 || enet_address_set_host(&addr, address.c_str()) != 0) {
        Fail("Host address could not be resolved"); return false;
    }
    auto peer = enet_host_connect(host, &addr, 3, ProtocolVersion);
    if (!peer) { Fail("Connection allocation failed"); return false; }
    peers[0].transport = peer; peers[0].since = enet_time_get();
    return true;
}
bool Session::Admit(uint16_t connection, const Party& party) {
    if (!ValidParty(party) || roster.size() + party.size() > settings.maxPlayers) return false;
    auto next = roster;
    for (size_t index = 0; index < party.size(); ++index) {
        unsigned racers = 0, characters = 0;
        for (auto p : next) { racers |= 1u << p.racer; characters |= 1u << p.character; }
        uint8_t racer = 0, character = party[index].character;
        while (racers & (1u << racer)) ++racer;
        while (characters & (1u << character)) character = (character + 1) % 8;
        next.push_back({uint16_t(racer + 1), connection, racer, uint8_t(index), character,
                       party[index].transmission, party[index].ready, true});
    }
    if (!ValidRoster(next)) return false;
    roster = std::move(next); ++revision; return true;
}
uint16_t Session::LocalParticipant() const {
    for (auto p : roster) if (p.connection == connectionId && p.controller == 0) return p.participant;
    return 0;
}
int Session::LocalCount() const {
    return int(std::count_if(roster.begin(), roster.end(), [this](auto p) { return p.connection == connectionId; }));
}
uint16_t Session::ParticipantForSlot(int slot) const {
    auto order = EngineOrder(roster, connectionId);
    for (uint16_t id = 1; id <= MaxRacers; ++id)
        if (std::find(order.begin(), order.end(), id) == order.end()) order.push_back(id);
    return slot >= 0 && slot < int(order.size()) ? order[slot] : 0;
}
int Session::EngineSlot(uint16_t participant) const {
    if (!participant || participant > MaxRacers) return -1;
    for (int slot = 0; slot < int(MaxRacers); ++slot) if (ParticipantForSlot(slot) == participant) return slot;
    return -1;
}
bool Session::HasPeer() const {
    return std::any_of(peers.begin(), peers.end(), [](const Peer& p) { return p.admitted; });
}
uint32_t Session::Rtt() const {
    uint32_t rtt = 0;
    for (const auto& p : peers) if (p.transport && p.admitted) rtt = std::max(rtt, p.transport->roundTripTime);
    return rtt;
}
bool Session::Send(Peer& p, Message m, bool reliable) {
    if (!p.transport || !p.crypto || !p.crypto->Ready()) return false;
    m.session = m.kind == Kind::Hello ? 0 : sessionId;
    m.race = m.kind == Kind::Hello ? 0 : raceId;
    try {
        uint8_t channel = Channel(m.kind);
        auto bytes = p.crypto->Seal(Encode(m), channel);
        if (!bytes.empty() && RawSend(p.transport, bytes, channel, reliable)) { ++sent; return true; }
        ++rejected; Fail("Packet queue failed");
    } catch (const std::exception&) { ++rejected; Fail("Packet encoding failed"); }
    return false;
}
void Session::Broadcast(Message m, bool reliable) {
    for (auto& p : peers) if (p.admitted) Send(p, m, reliable);
}
void Session::SendLobby(Peer& p, Kind kind) {
    Message m; m.kind = kind; m.content = contentHash; m.connection = p.connection;
    m.roster = roster; m.settings = settings; m.revision = revision; m.standings = standings; m.cupComplete = cupComplete;
    m.partyRevision = p.partyRevision;
    m.ready = phase == Phase::Lobby; m.phase = phase; m.ticket = tickets[p.connection];
    Send(p, m);
}
void Session::BroadcastLobby() { for (auto& p : peers) if (p.admitted) SendLobby(p, Kind::Lobby); }
void Session::Reject(Peer& p, const char* text) {
    Message m; m.kind = Kind::Reject; m.reason = text; Send(p, m);
    if (p.transport) enet_peer_disconnect_later(p.transport, 2);
    ++rejected;
}
void Session::Remove(Peer& p, bool leaving) {
    auto connection = p.connection;
    RetirePeer(p);
    tauntQueue.clear();
    if (!authority) {
        inputs = {};
        if (phase != Phase::Error) phase = Phase::Disconnected;
        if (error.empty()) error = "Host disconnected";
        if (!disconnectedAt) disconnectedAt = enet_time_get();
        reconnectAt = enet_time_get();
        return;
    }
    if (!connection) return;
    if (leaving && phase == Phase::Lobby) {
        roster.erase(std::remove_if(roster.begin(), roster.end(), [connection](auto r) { return r.connection == connection; }), roster.end());
        tickets[connection] = {}; reservationAt[connection] = 0;
        ++revision; BroadcastLobby(); return;
    }
    reservationAt[connection] = enet_time_get();
    for (auto& v : roster) if (v.connection == connection) { v.connected = v.ready = false; inputs[v.racer] = {}; }
    ++revision; BroadcastLobby();
}
void Session::ReceivePacket(Peer& p, const uint8_t* data, size_t size, uint8_t channel) {
    if (!p.crypto) { ++rejected; return; }
    if (p.keyPending || p.exchange.valid()) {
        if (authority && channel == 0 && p.pendingHello.empty() && size <= WireLimit)
            p.pendingHello.assign(data, data + size);
        else ++rejected;
        return;
    }
    if (!p.crypto->Ready()) {
        if (channel != 0 || size < 5 || std::memcmp(data, "SKK", 3) || data[3] != 3) { ++rejected; return; }
        if (authority && data[4] == 2 && size == 37) {
            std::array<uint8_t, 32> publicKey{}; std::copy_n(data + 5, 32, publicKey.begin());
            p.remoteKey = publicKey; p.remoteSalt = p.crypto->Salt(); p.keyPending = true;
        } else if (!authority && data[4] == 1 && size == 53 && phase == Phase::Connecting) {
            std::array<uint8_t, 32> publicKey{}; std::array<uint8_t, 16> salt{};
            std::copy_n(data + 5, 32, publicKey.begin()); std::copy_n(data + 37, 16, salt.begin());
            p.remoteKey = publicKey; p.remoteSalt = salt; p.keyPending = true;
        } else ++rejected;
        return;
    }
    std::vector<uint8_t> plain;
    if (!p.crypto->Open(data, size, channel, plain)) { ++rejected; return; }
    Message m;
    if (!Decode(plain.data(), plain.size(), m)) { ++rejected; return; }
    Receive(p, m, channel);
}
void Session::Poll() {
    if (!host) return;
    PollHandshakes();
    ENetEvent event{};
    for (int n = 0; n < 128 && enet_host_service(host, &event, 0) > 0; ++n) {
        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            Peer* entry = nullptr;
            if (authority) {
                if ((phase != Phase::Lobby && phase != Phase::Loading && phase != Phase::Racing && phase != Phase::Finished) ||
                    event.data != ProtocolVersion) { enet_peer_disconnect_now(event.peer, 2); continue; }
                if (!AllowHandshake(event.peer->address.host)) { enet_peer_disconnect_now(event.peer, 3); continue; }
                for (auto& p : peers) if (!p.transport) { entry = &p; break; }
                if (!entry) { enet_peer_disconnect_now(event.peer, 2); continue; }
                entry->transport = event.peer; entry->since = enet_time_get();
            } else if (peers[0].transport == event.peer) entry = &peers[0];
            if (!entry) { enet_peer_disconnect_now(event.peer, 2); continue; }
            entry->crypto = std::make_unique<SecureChannel>();
            if (!entry->crypto->Begin(authority, password)) { enet_peer_disconnect_now(event.peer, 2); Remove(*entry); continue; }
            enet_peer_timeout(event.peer, 8, 2000, 5000); enet_peer_ping_interval(event.peer, 250);
            if (authority && !RawSend(event.peer, KeyPacket(1, *entry->crypto), 0)) {
                enet_peer_disconnect_now(event.peer, 2); Remove(*entry); continue;
            }
            if (!authority && phase == Phase::Connecting) phaseAt = enet_time_get();
        } else {
            auto found = std::find_if(peers.begin(), peers.end(), [&event](const Peer& p) { return p.transport == event.peer; });
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                ++received;
                if (found != peers.end() && event.packet->dataLength <= WireLimit)
                    ReceivePacket(*found, event.packet->data, event.packet->dataLength, event.channelID);
                else ++rejected;
                enet_packet_destroy(event.packet);
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT && found != peers.end()) Remove(*found, event.data == 4);
        }
    }
    PollHandshakes();
    uint32_t now = enet_time_get();
    if (authority && phase == Phase::Lobby) {
        bool changed = false;
        for (uint16_t id = 2; id <= 8; ++id) if (reservationAt[id] && now - reservationAt[id] >= 30000) {
            roster.erase(std::remove_if(roster.begin(), roster.end(), [id](auto r) { return r.connection == id; }), roster.end());
            reservationAt[id] = 0; tickets[id] = {}; changed = true;
        }
        if (changed) { ++revision; BroadcastLobby(); }
    }
    for (auto& p : peers) if (p.transport && !p.admitted && now - p.since > 10000) {
        enet_peer_disconnect_now(p.transport, 2); Remove(p);
        if (!authority) error = "Connection or password check timed out";
    }
    if (phase == Phase::Loading && now - phaseAt > 30000) Fail("Race loading timed out");
    if (phase == Phase::Connecting && now - phaseAt > 10000) Fail("Connection or password check timed out");
    if (!authority && (phase == Phase::Disconnected || phase == Phase::Error) && resumeTicket != std::array<uint8_t,16>{} &&
        now - reconnectAt >= 1000 && now - disconnectedAt < 30000) Reconnect();
    enet_host_flush(host);
}
void Session::Receive(Peer& p, const Message& m, uint8_t channel) {
    if (channel != Channel(m.kind)) { ++rejected; return; }
    if (authority && m.kind == Kind::Hello && !p.admitted) {
        if (m.rules != RulesVersion || m.content != contentHash) { Reject(p, "Game content or race rules differ"); return; }
        uint16_t resumed = 0;
        if (m.ticket != std::array<uint8_t,16>{}) {
            for (uint16_t id = 2; id <= 8; ++id)
                if (crypto_verify16(tickets[id].data(), m.ticket.data()) == 0) resumed = id;
        }
        if (resumed) {
            auto count = std::count_if(roster.begin(), roster.end(), [resumed](auto r) { return r.connection == resumed; });
            if (count != int(m.party.size())) { Reject(p, "Reconnect party differs from reserved seats"); return; }
            for (auto& old : peers) if (&old != &p && old.admitted && old.connection == resumed) {
                enet_peer_disconnect_now(old.transport, 0); ResetPeer(old);
            }
            p.connection = resumed; p.admitted = true;
            reservationAt[resumed] = 0;
            for (auto& r : roster) if (r.connection == resumed) r.connected = true;
            ++revision; SendLobby(p, Kind::Welcome);
            if (phase == Phase::Finished && snapshot.kind == Kind::Finish) {
                SendSnapshot(p, snapshot);
                for (const auto& bytes : finalWorld) { Message world; world.kind = Kind::World; world.payload = bytes; Send(p, world); }
            }
            BroadcastLobby(); return;
        }
        if (phase != Phase::Lobby || m.ticket != std::array<uint8_t,16>{}) { Reject(p, "Race is underway or reconnect seat expired"); return; }
        uint16_t connection = 2;
        while (connection <= 8 && std::any_of(roster.begin(), roster.end(), [connection](auto r) { return r.connection == connection; })) ++connection;
        if (connection > 8 || !Admit(connection, m.party)) { Reject(p, "Not enough seats for your party"); return; }
        if (!SecureChannel::Random(tickets[connection].data(), tickets[connection].size())) {
            roster.erase(std::remove_if(roster.begin(), roster.end(), [connection](auto r) { return r.connection == connection; }), roster.end());
            Reject(p, "Connection ticket could not be allocated"); return;
        }
        p.connection = connection; p.admitted = true;
        reservationAt[connection] = 0;
        SendLobby(p, Kind::Welcome); BroadcastLobby(); return;
    }
    if (!authority && m.kind == Kind::Reject) { resumeTicket = {}; Fail(m.reason.c_str()); return; }
    if (!authority && m.kind == Kind::Welcome && phase == Phase::Connecting) {
        if (m.rules != RulesVersion || m.content != contentHash) { Fail("Game content or race rules differ"); return; }
        auto count = std::count_if(m.roster.begin(), m.roster.end(), [&m](auto r) { return r.connection == m.connection; });
        if (count != int(localParty.size())) { Fail("Server admitted an incomplete party"); return; }
        const bool sameRace = raceId == m.race;
        tauntQueue.clear();
        if (!sameRace) tauntSequence = receivedTauntSequence = 0;
        if (!sameRace) { receivedSnapshots = 0; snapshot = {}; assembling = {}; assembled = 0; worldQueue.clear(); }
        sessionId = m.session; raceId = m.race; connectionId = m.connection; p.connection = 1; p.admitted = true;
        resumeTicket = m.ticket; disconnectedAt = reconnectAt = 0; error.clear();
        roster = m.roster; settings = m.settings; revision = m.revision; standings = m.standings; cupComplete = m.cupComplete;
        partySent = partyAck = m.partyRevision;
        phase = m.phase;
        if (phase == Phase::Loading && sameRace && localLoaded) {
            Message loaded; loaded.kind = Kind::Loaded; loaded.round = settings.round; Send(p, loaded);
        }
        if (phase == Phase::Loading && !sameRace) localLoaded = false;
        return;
    }
    if (!p.admitted || !sessionId || m.session != sessionId) { ++rejected; return; }
    if (!authority && m.kind == Kind::Lobby) {
        if (m.connection != connectionId || m.rules != RulesVersion || m.content != contentHash ||
            (!Newer(m.revision, revision) && m.revision != revision)) { ++rejected; return; }
        roster = m.roster; settings = m.settings; revision = m.revision; standings = m.standings; cupComplete = m.cupComplete;
        if (Newer(m.partyRevision, partyAck) || m.partyRevision == partyAck) partyAck = m.partyRevision;
        if (phase == Phase::Finished && m.ready) phase = Phase::Lobby;
        return;
    }
    if (authority && m.kind == Kind::Ready && phase == Phase::Lobby) {
        for (auto& r : roster)
            if (r.connection == p.connection && (m.controller == 255 || r.controller == m.controller)) r.ready = m.ready;
        ++revision; BroadcastLobby(); return;
    }
    if (authority && m.kind == Kind::UpdateParty && phase == Phase::Lobby) {
        if (!Newer(m.partyRevision, p.partyRevision)) { ++rejected; return; }
        auto backup = roster;
        roster.erase(std::remove_if(roster.begin(), roster.end(), [&p](auto r) { return r.connection == p.connection; }), roster.end());
        if (!Admit(p.connection, m.party)) { roster = std::move(backup); Reject(p, "Party update does not fit"); return; }
        for (auto& r : roster) r.ready = false;
        p.partyRevision = m.partyRevision;
        ++revision; BroadcastLobby(); return;
    }
    if (!authority && m.kind == Kind::Load && (phase == Phase::Lobby || phase == Phase::Finished) && Newer(m.race, raceId)) {
        if (m.revision != revision) { Fail("Lobby roster changed during loading"); return; }
        settings = m.settings; raceId = m.race; localLoaded = false; phase = Phase::Loading; phaseAt = enet_time_get();
        tauntQueue.clear(); tauntSequence = receivedTauntSequence = 0;
        partySent = partyAck;
        receivedSnapshots = 0; snapshot = {}; assembling = {}; assembled = 0; worldQueue.clear(); return;
    }
    if (m.race != raceId) { ++rejected; return; }
    if (m.kind == Kind::Taunt) {
        if (authority || phase != Phase::Racing || p.connection != 1 || !TauntRosterMatches(m.taunt) ||
            !Newer(m.taunt.sequence, receivedTauntSequence)) { ++rejected; return; }
        const auto victim = std::find_if(roster.begin(), roster.end(), [&m](auto r) { return r.participant == m.taunt.victim; });
        if (victim->connection != connectionId) { ++rejected; return; }
        receivedTauntSequence = m.taunt.sequence; QueueTaunt(m.taunt); return;
    }
    if (!authority && m.kind == Kind::World && (phase == Phase::Racing || phase == Phase::Finished)) {
        if (worldQueue.size() == 32) worldQueue.erase(worldQueue.begin());
        worldQueue.push_back(m.payload); return;
    }
    if (authority && m.kind == Kind::Loaded && phase == Phase::Loading && m.round == settings.round) {
        p.loaded = true; TryBegin(); return;
    }
    if (!authority && m.kind == Kind::Begin && phase == Phase::Loading && localLoaded && m.round == settings.round) {
        phase = Phase::Racing; return;
    }
    if (authority && m.kind == Kind::Input && phase == Phase::Racing && m.round == settings.round) {
        auto r = std::find_if(roster.begin(), roster.end(), [&m, &p](auto v) {
            return v.participant == m.input.participant && v.connection == p.connection && v.connected;
        });
        if (r == roster.end() || !Newer(m.input.sequence, inputs[r->racer].sequence)) { ++rejected; return; }
        inputs[r->racer] = m.input; inputAt[r->racer] = enet_time_get(); return;
    }
    if (!authority && (m.kind == Kind::Snapshot || m.kind == Kind::Finish) &&
        (phase == Phase::Racing || phase == Phase::Finished) && m.round == settings.round &&
        (!receivedSnapshots || Newer(m.tick, snapshot.tick) || (m.kind == Kind::Finish && m.tick == snapshot.tick))) {
        if (phase == Phase::Finished && m.kind != Kind::Finish) { ++rejected; return; }
        if (!assembled || Newer(m.tick, assembling.tick) ||
            (m.tick == assembling.tick && m.kind == Kind::Finish && assembling.kind == Kind::Snapshot)) {
            assembling = m; assembling.karts.assign(MaxRacers, {}); assembled = 0;
        }
        if (m.tick != assembling.tick || m.parts != assembling.parts || m.kind != assembling.kind ||
            m.time != assembling.time || m.standings != assembling.standings) { ++rejected; return; }
        for (size_t i = 0; i < m.karts.size(); ++i) assembling.karts[m.part * KartsPerPacket + i] = m.karts[i];
        assembled |= 1u << m.part;
        if (assembled != (1u << m.parts) - 1) return;
        assembling.karts.erase(std::remove_if(assembling.karts.begin(), assembling.karts.end(), [](auto k) { return !k.participant; }),
                              assembling.karts.end());
        unsigned ids = 0;
        for (auto k : assembling.karts) {
            if (ids & (1u << k.participant)) { assembled = 0; ++rejected; return; }
            ids |= 1u << k.participant;
        }
        snapshot = assembling; assembled = 0; ++receivedSnapshots; lastSnapshotAt = enet_time_get();
        inputAck = m.acknowledgedInput; standings = m.standings; cupComplete = m.cupComplete;
        if (m.kind == Kind::Finish) phase = Phase::Finished;
        return;
    }
    ++rejected;
}
bool Session::SetSettings(const RaceSettings& next) {
    if (!authority || phase != Phase::Lobby || !next.Valid() || next.maxPlayers < roster.size()) return false;
    settings = next; settings.round = 0; for (auto& p : roster) p.ready = false;
    ++revision; BroadcastLobby(); return true;
}
void Session::SetReady(bool ready) { SetPlayerReady(255, ready); }
void Session::SetPlayerReady(uint8_t controller, bool ready) {
    if (phase != Phase::Lobby) return;
    if (controller != 255 && controller >= MaxLocalPlayers) return;
    if (authority) {
        for (auto& p : roster) if (p.connection == connectionId && (controller == 255 || p.controller == controller)) p.ready = ready;
        ++revision; BroadcastLobby();
    } else { Message m; m.kind = Kind::Ready; m.ready = ready; m.controller = controller; Send(peers[0], m); }
}
bool Session::UpdateParty(const Party& party) {
    if (phase != Phase::Lobby || !ValidParty(party)) return false;
    if (!authority) {
        Message m; m.kind = Kind::UpdateParty; m.party = party; m.partyRevision = partySent + 1;
        if (!Send(peers[0], m)) return false;
        partySent = m.partyRevision; localParty = party; return true;
    }
    auto backup = roster;
    roster.erase(std::remove_if(roster.begin(), roster.end(), [this](auto r) { return r.connection == connectionId; }), roster.end());
    if (!Admit(connectionId, party)) { roster = std::move(backup); return false; }
    localParty = party; for (auto& r : roster) r.ready = false;
    ++revision; BroadcastLobby(); return true;
}
bool Session::CanStart() const {
    if (!authority || roster.empty() || (phase != Phase::Lobby && phase != Phase::Finished)) return false;
    if (phase == Phase::Finished && cupComplete) return false;
    return std::all_of(roster.begin(), roster.end(), [this](auto p) { return p.connected && (phase == Phase::Finished || p.ready); });
}
bool Session::Start() {
    if (!CanStart()) return false;
    if (phase == Phase::Finished && settings.mode == 0) ++settings.round;
    ++raceId; localLoaded = false; receivedSnapshots = 0; snapshot = {}; assembled = 0; worldQueue.clear(); finalWorld.clear(); inputs = {}; inputAt = {};
    tauntQueue.clear(); tauntSequence = receivedTauntSequence = 0;
    for (auto& p : peers) p.loaded = false;
    Message m; m.kind = Kind::Load; m.settings = settings; m.revision = revision;
    Broadcast(m); phase = Phase::Loading; phaseAt = enet_time_get(); return true;
}
void Session::Loaded() {
    if (phase != Phase::Loading || localLoaded) return;
    localLoaded = true;
    if (authority) TryBegin();
    else { Message m; m.kind = Kind::Loaded; m.round = settings.round; Send(peers[0], m); }
}
void Session::TryBegin() {
    if (!localLoaded) return;
    for (auto p : roster) if (!p.connected) return;
    for (const auto& p : peers) if (p.admitted && !p.loaded) return;
    Message m; m.kind = Kind::Begin; m.round = settings.round; Broadcast(m); phase = Phase::Racing;
}
void Session::Submit(Input input) {
    if (phase != Phase::Racing) return;
    if (!input.participant) input.participant = LocalParticipant();
    auto r = std::find_if(roster.begin(), roster.end(), [this, &input](auto p) {
        return p.participant == input.participant && p.connection == connectionId;
    });
    if (r == roster.end()) return;
    if (authority) {
        if (Newer(input.sequence, inputs[r->racer].sequence)) { inputs[r->racer] = input; inputAt[r->racer] = enet_time_get(); }
    } else { Message m; m.kind = Kind::Input; m.round = settings.round; m.input = input; Send(peers[0], m, false); }
}
Input Session::InputFor(uint16_t participant) const {
    if (phase != Phase::Racing || participant < 1 || participant > MaxRacers ||
        enet_time_get() - inputAt[participant - 1] > 250) return {};
    return inputs[participant - 1];
}
Input Session::RemoteInput() const {
    for (auto p : roster) if (p.connection != connectionId) return InputFor(p.participant);
    return {};
}
uint32_t Session::SnapshotAge() const { return receivedSnapshots ? enet_time_get() - lastSnapshotAt : 0; }
void Session::PublishWorld(const std::vector<uint8_t>& payload) {
    if (!authority || (phase != Phase::Racing && phase != Phase::Finished) || payload.empty() || payload.size() > 1100) return;
    Message m; m.kind = Kind::World; m.payload = payload; Broadcast(m, false);
}
void Session::PublishWorldFrame(const std::vector<std::vector<uint8_t>>& chunks, bool final) {
    if (!authority || (phase != Phase::Racing && phase != Phase::Finished) || chunks.empty() || chunks.size() > 32 ||
        std::any_of(chunks.begin(), chunks.end(), [](const auto& bytes) { return bytes.empty() || bytes.size() > 1100; })) return;
    if (final || phase == Phase::Finished) finalWorld = chunks;
    for (const auto& bytes : chunks) { Message m; m.kind = Kind::World; m.payload = bytes; Broadcast(m, final); }
}
std::vector<std::vector<uint8_t>> Session::TakeWorld() {
    auto queue = std::move(worldQueue); worldQueue.clear(); return queue;
}
bool Session::TauntRosterMatches(const TauntEvent& e) const {
    if (!ValidTaunt(e)) return false;
    const auto attacker = std::find_if(roster.begin(), roster.end(), [&e](auto r) { return r.participant == e.attacker; });
    const auto victim = std::find_if(roster.begin(), roster.end(), [&e](auto r) { return r.participant == e.victim; });
    return attacker != roster.end() && victim != roster.end() && attacker->connected && victim->connected &&
           attacker->character == e.character;
}
void Session::QueueTaunt(const TauntEvent& event) {
    if (tauntQueue.size() == MaxQueuedTaunts) tauntQueue.erase(tauntQueue.begin());
    tauntQueue.push_back(event);
}
bool Session::PublishTaunt(uint16_t attacker, uint16_t victim, uint8_t character, float courseTime, uint32_t tick) {
    if (!authority || phase != Phase::Racing) return false;
    uint32_t sequence = tauntSequence + 1;
    if (!sequence) sequence = 1;
    TauntEvent event{sequence, attacker, victim, character, courseTime, tick};
    if (!TauntRosterMatches(event)) return false;
    const auto target = std::find_if(roster.begin(), roster.end(), [victim](auto r) { return r.participant == victim; });
    if (target->connection == connectionId) {
        tauntSequence = sequence; QueueTaunt(event); return true;
    }
    for (auto& p : peers) if (p.admitted && p.connection == target->connection) {
        Message m; m.kind = Kind::Taunt; m.taunt = event;
        if (!Send(p, m)) return false;
        tauntSequence = sequence; return true;
    }
    return false;
}
std::vector<TauntEvent> Session::TakeTaunts() {
    auto queue = std::move(tauntQueue); tauntQueue.clear();
    if (phase != Phase::Racing) queue.clear();
    return queue;
}
void Session::Publish(Message m, bool finish) {
    if (!authority || phase != Phase::Racing || m.karts.empty() || m.karts.size() > MaxRacers) return;
    m.kind = finish ? Kind::Finish : Kind::Snapshot; m.round = settings.round; m.race = raceId;
    if (finish) {
        static constexpr uint16_t points[] = {9,6,3,1,0,0,0,0};
        if (settings.mode == 0) for (auto k : m.karts)
            if (k.participant && k.participant <= MaxRacers && k.rank < MaxRacers) standings[k.participant - 1] += points[k.rank];
        cupComplete = settings.mode == 0 && settings.round == 3;
    }
    m.standings = standings; m.cupComplete = cupComplete; snapshot = m;
    for (auto& p : peers) if (p.admitted) SendSnapshot(p, m);
    if (finish) phase = Phase::Finished;
}
void Session::PublishPresentation(Message m) {
    if (!authority || phase != Phase::Finished || snapshot.kind != Kind::Finish ||
        m.karts.size() != snapshot.karts.size() || !Newer(m.tick, snapshot.tick)) return;
    unsigned seen = 0;
    for (auto& kart : m.karts) {
        if (!kart.participant || kart.participant > MaxRacers || (seen & (1u << kart.participant))) return;
        seen |= 1u << kart.participant;
        const auto result = std::find_if(snapshot.karts.begin(), snapshot.karts.end(), [&](const Kart& old) {
            return old.participant == kart.participant;
        });
        if (result == snapshot.karts.end()) return;
        kart.rank = result->rank; kart.lap = result->lap; kart.lapTimes = result->lapTimes;
    }
    // Movement continues behind results, but this race's scoring and finish data stay committed.
    m.kind = Kind::Finish; m.round = settings.round; m.race = raceId;
    m.standings = standings; m.cupComplete = cupComplete; snapshot = m;
    for (auto& p : peers) if (p.admitted) SendSnapshot(p, m, false);
}
void Session::SendSnapshot(Peer& p, const Message& m, bool reliableFinish) {
    if (m.karts.empty() || m.karts.size() > MaxRacers) return;
    uint32_t ack = 0;
    for (auto r : roster) if (r.connection == p.connection && r.controller == 0) ack = inputs[r.racer].sequence;
    for (size_t start = 0; start < m.karts.size(); start += KartsPerPacket) {
        Message part = m; part.parts = uint8_t((m.karts.size() + KartsPerPacket - 1) / KartsPerPacket);
        part.part = uint8_t(start / KartsPerPacket); part.acknowledgedInput = ack;
        part.karts.assign(m.karts.begin() + start, m.karts.begin() + std::min(start + KartsPerPacket, m.karts.size()));
        Send(p, part, m.kind == Kind::Finish && reliableFinish);
    }
}
bool Session::ReturnToLobby() {
    if (!authority || phase != Phase::Finished) return false;
    for (const auto& p : roster) if (!p.connected) { tickets[p.connection] = {}; reservationAt[p.connection] = 0; }
    roster.erase(std::remove_if(roster.begin(), roster.end(), [](auto p) { return !p.connected; }), roster.end());
    settings.round = 0; standings = {}; cupComplete = false; finalWorld.clear();
    tauntQueue.clear();
    for (auto& p : roster) p.ready = false;
    phase = Phase::Lobby; ++revision; BroadcastLobby(); return true;
}
bool Session::Reconnect() {
    if (authority || !host || remoteAddress.empty() || resumeTicket == std::array<uint8_t,16>{}) return false;
    tauntQueue.clear();
    for (auto& p : peers) {
        if (p.transport) enet_peer_disconnect_now(p.transport, 0);
        RetirePeer(p);
    }
    ENetAddress address{}; address.port = remotePort;
    if (enet_address_set_host(&address, remoteAddress.c_str()) != 0) return false;
    auto peer = enet_host_connect(host, &address, 3, ProtocolVersion);
    if (!peer) return false;
    peers[0].transport = peer; peers[0].since = enet_time_get();
    phase = Phase::Connecting; reconnectAt = phaseAt = enet_time_get(); return true;
}
}
