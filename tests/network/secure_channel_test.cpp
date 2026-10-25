#include "SecureChannel.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>

using KartNet::SecureChannel;

namespace {

size_t checks = 0;

void Check(bool condition, const char* text) {
    ++checks;
    if (!condition) throw std::runtime_error(text);
}

void RandomTokens() {
    Check(SecureChannel::Random(nullptr, 0), "Empty randomness request");
    Check(!SecureChannel::Random(nullptr, 16), "Null randomness destination rejected");
    std::array<uint8_t, 18> first{};
    first.front() = 0x5a; first.back() = 0xa5;
    Check(SecureChannel::Random(first.data() + 1, 16), "128-bit resume ticket OS randomness");
    Check(first.front() == 0x5a && first.back() == 0xa5, "Random ticket buffer bounds");
    std::array<uint8_t, 16> second{};
    Check(SecureChannel::Random(second.data(), second.size()), "Second private ticket randomness");
    Check(!std::equal(second.begin(), second.end(), first.begin() + 1), "Independent random tickets");
    const auto unchanged = first;
    Check(SecureChannel::Random(first.data(), 0) && first == unchanged, "Zero length does not write");
    Check(!SecureChannel::Random(first.data(), SecureChannel::MaxRandomBytes + 1) && first == unchanged,
          "Oversized random request rejected before buffer access");
    Check(!SecureChannel::Random(first.data(), std::numeric_limits<size_t>::max()) && first == unchanged,
          "Huge random request rejected before buffer access");
    std::array<uint8_t, SecureChannel::MaxRandomBytes + 2> maximum{};
    maximum.front() = 0x5a; maximum.back() = 0xa5;
    Check(SecureChannel::Random(maximum.data() + 1, SecureChannel::MaxRandomBytes), "Maximum random length");
    Check(maximum.front() == 0x5a && maximum.back() == 0xa5, "Maximum random buffer bounds");
}

struct Peers {
    SecureChannel host;
    SecureChannel client;

    void Begin(const std::string& hostPassword = "race password",
               const std::string& clientPassword = "race password") {
        Check(host.Begin(true, hostPassword), "Host OS randomness / Begin");
        Check(client.Begin(false, clientPassword), "Client OS randomness / Begin");
        Check(!host.Ready() && !client.Ready(), "Begin is not Ready");
    }

    void Establish() {
        Check(host.Establish(client.PublicKey(), host.Salt()), "Host Establish");
        Check(client.Establish(host.PublicKey(), host.Salt()), "Client Establish");
        Check(host.Ready() && client.Ready(), "Both keys derived");
    }

    void Start(const std::string& password = "race password") {
        Begin(password, password);
        Establish();
    }
};

void Rejected(SecureChannel& receiver, const uint8_t* bytes, size_t len, uint8_t channel,
              const char* text) {
    std::vector<uint8_t> plain{9, 8, 7};
    Check(!receiver.Open(bytes, len, channel, plain) && plain.empty(), text);
}

void Rejected(SecureChannel& receiver, const std::vector<uint8_t>& packet, uint8_t channel,
              const char* text) {
    Rejected(receiver, packet.data(), packet.size(), channel, text);
}

void Accepted(SecureChannel& receiver, const std::vector<uint8_t>& packet, uint8_t channel,
              const std::vector<uint8_t>& expected) {
    std::vector<uint8_t> plain{0xff};
    Check(receiver.Open(packet.data(), packet.size(), channel, plain) && plain == expected,
          "Authenticated payload roundtrip");
}

void RoundTripsAndBounds(Peers& peers) {
    Check(SecureChannel::MaxDatagram <= 1300, "Datagram ceiling");
    for (uint8_t channel = 0; channel < SecureChannel::ChannelCount; ++channel) {
        for (size_t size : {size_t(0), size_t(1), size_t(37), SecureChannel::MaxPlaintext}) {
            std::vector<uint8_t> plain(size);
            for (size_t i = 0; i < size; ++i) plain[i] = static_cast<uint8_t>(i * 79 + channel);
            const auto fromHost = peers.host.Seal(plain, channel);
            const auto fromClient = peers.client.Seal(plain, channel);
            Check(fromHost.size() == size + SecureChannel::Overhead &&
                  fromClient.size() == fromHost.size(), "Exact wire overhead including empty payload");
            Check(fromHost != fromClient, "Directional key / nonce separation");
            Accepted(peers.client, fromHost, channel, plain);
            Accepted(peers.host, fromClient, channel, plain);
        }
    }
    const std::vector<uint8_t> payload(128, 0x71);
    const auto first = peers.host.Seal(payload, 0);
    const auto second = peers.host.Seal(payload, 0);
    Check(first != second && !std::equal(first.begin() + SecureChannel::HeaderSize,
          first.end(), second.begin() + SecureChannel::HeaderSize), "Fresh nonce per datagram");
    Rejected(peers.host, first, 0, "Reflection fails authentication");
    Accepted(peers.client, first, 0, payload);
    Accepted(peers.client, second, 0, payload);
    const auto sameOtherChannel = peers.host.Seal(payload, 1);
    Check(!std::equal(second.begin() + SecureChannel::HeaderSize, second.end(),
          sameOtherChannel.begin() + SecureChannel::HeaderSize), "Channel key separation");
    Accepted(peers.client, sameOtherChannel, 1, payload);
    Check(peers.host.Seal(std::vector<uint8_t>(SecureChannel::MaxPlaintext + 1), 0).empty(),
          "Oversized plaintext rejected");
    Check(peers.host.Seal({}, 3).empty() && peers.host.Seal({}, 255).empty(), "Invalid send channel");
    Rejected(peers.client, first, 3, "Invalid receive channel");
    Rejected(peers.client, first, 255, "Invalid receive channel byte");
}

void TamperAndMalformed(Peers& peers) {
    const std::vector<uint8_t> payload(SecureChannel::MaxPlaintext, 0xa5);
    const auto packet = peers.host.Seal(payload, 0);
    // Every header, ciphertext and tag byte is covered. Forged high counters
    // must not poison replay state and block the original authenticated packet.
    for (size_t i = 0; i < packet.size(); ++i) {
        auto bad = packet;
        bad[i] ^= 0x80;
        Rejected(peers.client, bad, 0, "Single byte corruption rejected");
    }
    for (size_t len = 0; len < packet.size(); ++len)
        Rejected(peers.client, packet.data(), len, 0, "All truncation lengths rejected");
    auto extra = packet;
    extra.push_back(0);
    Rejected(peers.client, extra, 0, "Trailing byte rejected");
    extra.resize(1300);
    Rejected(peers.client, extra, 0, "1300-byte encrypted packet exceeds helper ceiling");
    Rejected(peers.client, packet.data(), std::numeric_limits<size_t>::max(), 0, "Huge length rejected first");
    Rejected(peers.client, nullptr, packet.size(), 0, "Null input rejected");
    Rejected(peers.client, packet, 1, "External channel binding");
    auto relabel = packet;
    relabel[5] = 1;
    Rejected(peers.client, relabel, 1, "Forged embedded and external channel binding");
    auto maximumCounter = packet;
    std::fill(maximumCounter.begin() + 8, maximumCounter.begin() + 16, uint8_t(0xff));
    Rejected(peers.client, maximumCounter, 0, "Terminal counter rejected");
    Accepted(peers.client, packet, 0, payload);
    Rejected(peers.client, packet, 0, "Authenticated replay rejected");

    const auto empty = peers.host.Seal({}, 2);
    Accepted(peers.client, empty, 2, {});
    Rejected(peers.client, empty, 2, "Empty replay rejected");
    auto aliased = peers.host.Seal({1, 0, 2}, 2);
    Check(peers.client.Open(aliased.data(), aliased.size(), 2, aliased) &&
          aliased == std::vector<uint8_t>({1, 0, 2}), "Input/output alias roundtrip");
    aliased = peers.host.Seal({3, 4}, 2);
    aliased.back() ^= 1;
    Check(!peers.client.Open(aliased.data(), aliased.size(), 2, aliased) && aliased.empty(),
          "Input/output alias rejection");

    std::mt19937 rng(0x534b5343);
    for (size_t i = 0; i < 5000; ++i) {
        std::vector<uint8_t> bad(rng() % 1401);
        for (auto& byte : bad) byte = static_cast<uint8_t>(rng());
        if (bad.size() >= SecureChannel::HeaderSize && i % 2 == 0) {
            std::copy_n(packet.data(), SecureChannel::HeaderSize, bad.data());
            bad[8] = static_cast<uint8_t>(rng());
        }
        Rejected(peers.client, bad, static_cast<uint8_t>(i % 4), "Malformed corpus rejected");
    }
    Accepted(peers.client, peers.host.Seal({8}, 0), 0, {8});
}

void ReplayWindow(Peers& peers) {
    std::vector<std::vector<uint8_t>> packets;
    for (uint8_t i = 0; i < 80; ++i) packets.push_back(peers.host.Seal({i}, 1));
    Accepted(peers.client, packets[79], 1, {79});
    Accepted(peers.client, packets[16], 1, {16}); // Exactly 63 behind the newest.
    Accepted(peers.client, packets[78], 1, {78});
    Accepted(peers.client, packets[17], 1, {17});
    Rejected(peers.client, packets[15], 1, "Age 64 rejected");
    Rejected(peers.client, packets[0], 1, "Old packet rejected");
    Rejected(peers.client, packets[16], 1, "Out-of-order duplicate rejected");
    Rejected(peers.client, packets[79], 1, "Newest duplicate rejected");
    Accepted(peers.client, peers.host.Seal({80}, 1), 1, {80});
    Rejected(peers.client, packets[16], 1, "Window advances past former oldest");
    Accepted(peers.client, packets[18], 1, {18});
    // A gap >= 64 discards the entire old mask without undefined shifts.
    std::vector<uint8_t> afterGap;
    for (size_t i = 0; i < 70; ++i) afterGap = peers.host.Seal({91}, 1);
    Accepted(peers.client, afterGap, 1, {91});
    Rejected(peers.client, packets[78], 1, "Large replay-window advance");
    Accepted(peers.client, peers.host.Seal({33}, 2), 2, {33});
    Accepted(peers.host, peers.client.Seal({34}, 1), 1, {34});
}

void LifecycleAndRekey(Peers& peers) {
    const auto oldPublic = peers.host.PublicKey();
    const auto oldSalt = peers.host.Salt();
    const auto oldHostPacket = peers.host.Seal({42}, 0);
    const auto oldClientPacket = peers.client.Seal({43}, 0);
    Check(!peers.host.Establish(peers.client.PublicKey(), peers.host.Salt()) && peers.host.Ready(),
          "Second Establish leaves active session intact");
    Accepted(peers.client, oldHostPacket, 0, {42});
    const auto next = peers.host.Seal({44}, 0);
    Accepted(peers.client, next, 0, {44});
    Rejected(peers.client, oldHostPacket, 0, "Second Establish did not reset replay window");

    peers.Begin();
    Check(peers.host.PublicKey() != oldPublic && peers.host.Salt() != oldSalt,
          "Rekey obtains fresh ephemeral key and salt");
    Rejected(peers.client, oldHostPacket, 0, "No decryption during handshake");
    peers.Establish();
    Rejected(peers.client, oldHostPacket, 0, "Prior session rejected after rekey");
    Rejected(peers.host, oldClientPacket, 0, "Prior reverse session rejected after rekey");
    Accepted(peers.client, peers.host.Seal({45}, 0), 0, {45});
    const auto beforeMove = peers.host.Seal({46}, 0);
    SecureChannel moved(std::move(peers.host));
    Check(moved.Ready() && !peers.host.Ready(), "Move clears source keys");
    Check(peers.host.PublicKey() == std::array<uint8_t, 32>{}, "Moved-from public state erased");
    Accepted(peers.client, beforeMove, 0, {46});
    Accepted(peers.client, moved.Seal({47}, 0), 0, {47});
    peers.host = std::move(moved);
    Check(peers.host.Ready() && !moved.Ready(), "Move assignment clears source keys");
    Accepted(peers.client, peers.host.Seal({48}, 0), 0, {48});
    peers.host.Reset();
    Check(!peers.host.Ready() && peers.host.PublicKey() == std::array<uint8_t, 32>{} &&
          peers.host.Salt() == std::array<uint8_t, 16>{}, "Reset clears public state");
    Check(peers.host.Seal({1}, 0).empty(), "No send after reset");
    Rejected(peers.host, oldClientPacket, 0, "No receive after reset");
}

void PasswordAndTranscript() {
    Peers peers;
    peers.Begin("host secret", "different secret");
    peers.Establish();
    Rejected(peers.client, peers.host.Seal({1}, 0), 0, "Password mismatch host-to-client");
    Rejected(peers.host, peers.client.Seal({2}, 0), 0, "Password mismatch client-to-host");
    peers.Start("");
    const auto encrypted = peers.client.Seal({1, 2, 3}, 0);
    Accepted(peers.host, encrypted, 0, {1, 2, 3});
    Rejected(peers.host, std::vector<uint8_t>{1, 2, 3}, 0, "Empty password has no plaintext fallback");
    for (bool hostEmpty : {false, true}) {
        peers.Begin(hostEmpty ? "" : "nonempty", hostEmpty ? "nonempty" : "");
        peers.Establish();
        Rejected(peers.client, peers.host.Seal({1}, 0), 0, "Empty/nonempty mismatch host-to-client");
        Rejected(peers.host, peers.client.Seal({2}, 0), 0, "Empty/nonempty mismatch client-to-host");
    }
    peers.Start(std::string("a\0b", 3));
    Accepted(peers.host, peers.client.Seal({3}, 0), 0, {3});

    peers.Begin();
    auto alteredSalt = peers.host.Salt();
    alteredSalt[0] ^= 1;
    Check(peers.host.Establish(peers.client.PublicKey(), peers.host.Salt()), "Host transcript setup");
    Check(peers.client.Establish(peers.host.PublicKey(), alteredSalt), "Client altered transcript keys");
    Rejected(peers.host, peers.client.Seal({4}, 0), 0, "Salt transcript binding");
}

void InvalidKeysAndHandshake() {
    SecureChannel channel;
    Check(!channel.Ready() && channel.Seal({}, 0).empty(), "Default inactive");
    Rejected(channel, nullptr, 0, 0, "Default receive inactive");
    Check(!channel.Establish({}, {}), "Establish before Begin");
    std::array<uint8_t, 32> zero{};
    std::array<uint8_t, 32> one{};
    one[0] = 1;
    std::array<uint8_t, 32> prime;
    prime.fill(0xff); prime[0] = 0xed; prime[31] = 0x7f;
    auto primeMinusOne = prime; --primeMinusOne[0];
    auto primePlusOne = prime; ++primePlusOne[0];
    auto allOnes = prime; allOnes.fill(0xff);
    auto highBit = one; highBit[31] = 0x80;
    for (bool server : {false, true}) {
        for (const auto& bad : {zero, one, primeMinusOne, prime, primePlusOne, allOnes, highBit}) {
            Check(channel.Begin(server, "secret"), "Invalid key test Begin");
            Check(!channel.Establish(bad, channel.Salt()) && !channel.Ready(),
                  "Zero, low-order or noncanonical public key rejected");
            Check(channel.PublicKey() == zero, "Failed handshake clears state");
            Check(!channel.Establish(bad, {}), "Failed handshake requires fresh Begin");
        }
    }
    Check(channel.Begin(true, "secret"), "Reflection Begin");
    Check(!channel.Establish(channel.PublicKey(), channel.Salt()), "Self public key rejected");
    Peers peers;
    peers.Begin();
    auto alteredSalt = peers.host.Salt(); alteredSalt[0] ^= 1;
    Check(!peers.host.Establish(peers.client.PublicKey(), alteredSalt) && !peers.host.Ready(),
          "Server rejects salt differing from its offer");
    Check(channel.Begin(true, std::string(SecureChannel::MaxPasswordBytes, 'x')), "Maximum password size");
    Check(!channel.Begin(true, std::string(SecureChannel::MaxPasswordBytes + 1, 'x')) && !channel.Ready(),
          "Oversized password rejected and previous keys discarded");
    Check(channel.PublicKey() == zero, "Oversized password clears state");
}

} // namespace

int main() {
    static_assert(!std::is_copy_constructible<SecureChannel>::value, "Sessions must not duplicate nonce state");
    static_assert(!std::is_copy_assignable<SecureChannel>::value, "Sessions must not duplicate key state");
    static_assert(std::is_nothrow_move_constructible<SecureChannel>::value, "Peer containers can move sessions");
    try {
        RandomTokens();
        InvalidKeysAndHandshake();
        Peers peers;
        peers.Start();
        RoundTripsAndBounds(peers);
        TamperAndMalformed(peers);
        ReplayWindow(peers);
        LifecycleAndRekey(peers);
        PasswordAndTranscript();
        std::cout << "SecureChannel: " << checks << " checks passed\n";
    } catch (const std::exception& ex) {
        std::cerr << "SecureChannel: " << ex.what() << '\n';
        return 1;
    }
}
