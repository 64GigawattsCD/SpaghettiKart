#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace KartNet {

// Per-peer datagram encryption, not a PAKE or a public-server identity protocol.
// The caller admits a peer only after its encrypted Hello authenticates, rate
// limits handshake attempts, and keeps application traffic off the raw channel.
// Exclusive ownership: Establish may run on a worker, synchronized before use.
class SecureChannel {
  public:
    static constexpr size_t ChannelCount = 3;
    static constexpr size_t MaxPlaintext = 1200;
    static constexpr size_t HeaderSize = 16;
    static constexpr size_t TagSize = 16;
    static constexpr size_t Overhead = HeaderSize + TagSize;
    static constexpr size_t MaxDatagram = MaxPlaintext + Overhead;
    static constexpr size_t MaxPasswordBytes = 256;
    static constexpr size_t ReplayWindowSize = 64;
    static constexpr size_t MaxRandomBytes = 4096;

    SecureChannel() noexcept = default;
    ~SecureChannel();
    SecureChannel(const SecureChannel&) = delete;
    SecureChannel& operator=(const SecureChannel&) = delete;
    SecureChannel(SecureChannel&& other) noexcept;
    SecureChannel& operator=(SecureChannel&& other) noexcept;

    // OS CSPRNG for private session tokens, never PublicKey/Salt-derived.
    // Zero length succeeds (including nullptr); other requests need a buffer
    // and length <= MaxRandomBytes. An entropy failure erases the output.
    static bool Random(uint8_t* bytes, size_t length);

    // Begin always discards the previous session. Each server peer needs its own
    // Begin (ephemeral key + salt). Nonempty passwords go through Argon2id;
    // an empty password uses a fixed zero password key, with fresh DH + salt.
    // Password bytes are copied only to RAM and erased after Establish/Reset.
    bool Begin(bool server, const std::string& password);
    const std::array<uint8_t, 32>& PublicKey() const { return publicKey_; }
    const std::array<uint8_t, 16>& Salt() const { return salt_; }

    // Server passes its offered salt; client passes the received server salt.
    // One attempt per Begin: a failed attempt requires another Begin. Calling
    // Establish on an established session fails without resetting its counters.
    bool Establish(const std::array<uint8_t, 32>& remotePublic,
                   const std::array<uint8_t, 16>& salt);
    std::vector<uint8_t> Seal(const std::vector<uint8_t>& plain, uint8_t channel);
    // Failure empties plain. Empty plaintext is valid (a 32-byte datagram).
    bool Open(const uint8_t* bytes, size_t len, uint8_t channel, std::vector<uint8_t>& plain);
    // Derived keys exist; peer authentication still requires encrypted Hello.
    bool Ready() const { return ready_; }
    void Reset() noexcept;

  private:
    struct ChannelState {
        std::array<uint8_t, 32> sendKey{};
        std::array<uint8_t, 32> receiveKey{};
        std::array<uint8_t, 16> sendPrefix{};
        std::array<uint8_t, 16> receivePrefix{};
        uint64_t nextSend = 0;
        uint64_t highestReceived = 0;
        uint64_t receivedMask = 0;
        bool receivedAny = false;
    };

    std::array<uint8_t, 32> secretKey_{};
    std::array<uint8_t, 32> publicKey_{};
    std::array<uint8_t, 16> salt_{};
    std::array<uint8_t, MaxPasswordBytes> password_{};
    size_t passwordSize_ = 0;
    std::array<ChannelState, ChannelCount> channels_{};
    bool server_ = false;
    bool begun_ = false;
    bool ready_ = false;
};

} // namespace KartNet
