#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Protocol.h"
#include <enet/enet.h>
#include <string>
#include <memory>
#include <future>
#include "SecureChannel.h"

namespace KartNet {
constexpr size_t MaxQueuedTaunts = 32;
class Session {
public:
    ~Session();
    bool Host(uint16_t port, uint64_t content, uint8_t character, const char* bindAddress = nullptr);
    bool Join(const std::string& address, uint16_t port, uint64_t content, uint8_t character);
    bool Host(uint16_t port, uint64_t content, const Party& party, const RaceSettings& settings,
              const std::string& password = {}, const char* bindAddress = nullptr);
    bool Join(const std::string& address, uint16_t port, uint64_t content, const Party& party,
              const std::string& password = {});
    bool Local(uint64_t content, const Party& party, const RaceSettings& settings);
    void Close();
    void Poll();
    bool Start();
    void Loaded();
    void Submit(Input input);
    void Publish(Message snapshot, bool finish = false);
    void PublishPresentation(Message snapshot);
    void PublishWorld(const std::vector<uint8_t>& payload);
    void PublishWorldFrame(const std::vector<std::vector<uint8_t>>& chunks, bool final = false);
    std::vector<std::vector<uint8_t>> TakeWorld();
    bool PublishTaunt(uint16_t attacker, uint16_t victim, uint8_t character, float courseTime, uint32_t tick);
    std::vector<TauntEvent> TakeTaunts();
    Input RemoteInput() const;
    Input InputFor(uint16_t participant) const;
    void SetReady(bool ready);
    void SetPlayerReady(uint8_t controller, bool ready);
    bool UpdateParty(const Party& party);
    bool PartyUpdatePending() const { return !authority && partySent != partyAck; }
    bool SetSettings(const RaceSettings& settings);
    bool ReturnToLobby();
    bool Reconnect();
    bool CanStart() const;
    uint16_t LocalConnection() const { return connectionId; }
    uint16_t LocalParticipant() const;
    int LocalCount() const;
    int EngineSlot(uint16_t participant) const;
    uint16_t ParticipantForSlot(int slot) const;
    uint64_t SessionId() const { return sessionId; }
    int RaceIndex() const { return settings.round; }
    uint32_t RaceEpoch() const { return raceId; }
    bool CupComplete() const { return cupComplete; }
    bool LocalOnly() const { return localOnly; }
    bool PasswordRequired() const { return !password.empty(); }
    bool IsHost() const { return authority; }
    bool Active() const { return host != nullptr || localOnly; }
    bool HasPeer() const;
    Phase phase = Phase::Offline;
    std::string error;
    std::vector<Identity> roster;
    RaceSettings settings;
    std::array<uint16_t, MaxRacers> standings{};
    Message snapshot;
    uint32_t receivedSnapshots = 0, rejected = 0, sent = 0, received = 0, inputAck = 0;
    uint32_t SnapshotAge() const;
    uint32_t Rtt() const;
private:
    friend struct SessionTestAccess;
    struct Peer {
        ENetPeer* transport = nullptr;
        std::unique_ptr<SecureChannel> crypto;
        uint16_t connection = 0;
        uint32_t since = 0;
        uint32_t partyRevision = 0;
        bool admitted = false, loaded = false;
        bool keyPending = false;
        std::array<uint8_t, 32> remoteKey{};
        std::array<uint8_t, 16> remoteSalt{};
        std::future<bool> exchange;
        std::vector<uint8_t> pendingHello;
    };
    ENetHost* host = nullptr;
    std::array<Peer, 8> peers;
    std::array<Peer, 2> retiredExchanges;
    struct HandshakeRate { uint32_t address = 0, since = 0; unsigned count = 0; };
    std::array<HandshakeRate, 32> handshakeRates{};
    uint32_t handshakeSince = 0;
    unsigned handshakeAttempts = 0;
    bool authority = false, localOnly = false, localLoaded = false, cupComplete = false;
    bool transportInitialized = false;
    uint64_t contentHash = 0, sessionId = 0;
    uint16_t connectionId = 0;
    uint32_t revision = 0, raceId = 0, lastSnapshotAt = 0, phaseAt = 0;
    uint32_t partySent = 0, partyAck = 0;
    Party localParty;
    std::string password;
    std::string remoteAddress;
    uint16_t remotePort = 0;
    uint32_t reconnectAt = 0, disconnectedAt = 0;
    std::array<uint8_t, 16> resumeTicket{};
    std::array<std::array<uint8_t, 16>, 9> tickets{};
    std::array<uint32_t, 9> reservationAt{};
    std::array<Input, MaxRacers> inputs{};
    std::array<uint32_t, MaxRacers> inputAt{};
    Message assembling;
    unsigned assembled = 0;
    std::vector<std::vector<uint8_t>> worldQueue;
    std::vector<std::vector<uint8_t>> finalWorld;
    std::vector<TauntEvent> tauntQueue;
    uint32_t tauntSequence = 0, receivedTauntSequence = 0;
    bool TauntRosterMatches(const TauntEvent& event) const;
    void QueueTaunt(const TauntEvent& event);
    bool Open(bool server, uint16_t port, uint64_t content, const Party& party,
              const std::string& password, const char* bindAddress);
    bool Admit(uint16_t connection, const Party& party);
    bool Send(Peer& peer, Message message, bool reliable = true);
    void Broadcast(Message message, bool reliable = true);
    void Receive(Peer& peer, const Message& message, uint8_t channel);
    void ReceivePacket(Peer& peer, const uint8_t* bytes, size_t length, uint8_t channel);
    void SendLobby(Peer& peer, Kind kind);
    void BroadcastLobby();
    void Reject(Peer& peer, const char* text);
    void Remove(Peer& peer, bool leaving = false);
    static void ResetPeer(Peer& peer);
    void RetirePeer(Peer& peer);
    bool AllowHandshake(uint32_t address);
    void PollHandshakes();
    void SendSnapshot(Peer& peer, const Message& snapshot, bool reliableFinish = true);
    void Fail(const char* message);
    void TryBegin();
};
const char* PhaseName(Phase phase);
}
