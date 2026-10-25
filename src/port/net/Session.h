#pragma once
#include "Protocol.h"
#include <enet/enet.h>
#include <string>

namespace KartNet {
class Session {
public:
    ~Session();
    bool Host(uint16_t port, uint64_t content, uint8_t character, const char* bindAddress = nullptr);
    bool Join(const std::string& address, uint16_t port, uint64_t content, uint8_t character);
    void Close();
    void Poll();
    bool Start();
    void Loaded();
    void Submit(Input input);
    void Publish(Message snapshot, bool finish = false);
    Input RemoteInput() const;
    bool IsHost() const { return authority; }
    bool Active() const { return host != nullptr; }
    bool HasPeer() const { return admitted; }
    uint16_t LocalParticipant() const { return authority ? 1 : 2; }
    Phase phase = Phase::Offline;
    std::string error;
    std::array<Identity, 2> roster{};
    Message snapshot;
    uint32_t receivedSnapshots = 0, rejected = 0, sent = 0, received = 0, inputAck = 0;
    uint32_t SnapshotAge() const;
    uint32_t Rtt() const { return peer ? peer->roundTripTime : 0; }
private:
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;
    bool authority = false, admitted = false, localLoaded = false, remoteLoaded = false;
    bool transportInitialized = false;
    uint64_t contentHash = 0, sessionId = 0;
    uint8_t chosenCharacter = 0;
    Input remoteInput;
    uint32_t lastInputAt = 0, lastSnapshotAt = 0, phaseAt = 0;
    bool Open(bool server, uint16_t port, uint64_t content, uint8_t character, const char* bindAddress = nullptr);
    void Send(Message message, bool reliable = true);
    void Receive(const Message& message, uint8_t channel);
    void Fail(const char* message);
    void TryBegin();
};
const char* PhaseName(Phase phase);
}
