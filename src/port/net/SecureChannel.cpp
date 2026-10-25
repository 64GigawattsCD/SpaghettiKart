#include "SecureChannel.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <new>
#include <utility>
#include <monocypher.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#if defined(__linux__) || defined(__ANDROID__)
#include <sys/random.h>
#endif
#endif

namespace KartNet {
namespace {

constexpr uint8_t Magic[] = {'S', 'K', 'S', 'C'};
constexpr uint8_t Version = 1;
constexpr uint32_t ArgonBlocks = 65536; // 64 MiB, three passes, one lane.
constexpr uint32_t ArgonPasses = 3;
constexpr char ExtractDomain[] = "SpaghettiKart.SecureChannel.v1.extract";
constexpr char ClientDomain[] = "SpaghettiKart.SecureChannel.v1.client-to-server";
constexpr char ServerDomain[] = "SpaghettiKart.SecureChannel.v1.server-to-client";

template <size_t N> struct SecretBytes {
    std::array<uint8_t, N> bytes{};
    ~SecretBytes() { crypto_wipe(bytes.data(), bytes.size()); }
};

bool RandomBytes(uint8_t* bytes, size_t size) {
#if defined(_WIN32)
    if (size > std::numeric_limits<ULONG>::max()) return false;
    return BCryptGenRandom(nullptr, bytes, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
    size_t done = 0;
#if defined(__linux__) || defined(__ANDROID__)
    while (done < size) {
        const auto count = getrandom(bytes + done, size - done, 0);
        if (count > 0) {
            done += static_cast<size_t>(count);
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else if (count < 0 && errno == ENOSYS) {
            break;
        } else {
            return false;
        }
    }
    if (done == size) return true;
#endif
    int fd;
    do { fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC); } while (fd < 0 && errno == EINTR);
    if (fd < 0) return false;
    while (done < size) {
        const auto count = read(fd, bytes + done, size - done);
        if (count > 0) {
            done += static_cast<size_t>(count);
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            close(fd);
            return false;
        }
    }
    close(fd);
    return true;
#endif
}

// Our peers emit canonical X25519 coordinates. Also reject low-order points
// below by checking the DH output, not just the encoding of the public key.
bool CanonicalPublicKey(const std::array<uint8_t, 32>& key) {
    constexpr std::array<uint8_t, 32> Zero{};
    if (crypto_verify32(key.data(), Zero.data()) == 0) return false;
    std::array<uint8_t, 32> prime;
    prime.fill(0xff);
    prime[0] = 0xed;
    prime[31] = 0x7f;
    for (size_t i = key.size(); i-- > 0;) {
        if (key[i] != prime[i]) return key[i] < prime[i];
    }
    return false;
}

void StoreCounter(uint8_t* bytes, uint64_t counter) {
    for (size_t i = 0; i < 8; ++i) bytes[i] = static_cast<uint8_t>(counter >> (8 * i));
}

uint64_t LoadCounter(const uint8_t* bytes) {
    uint64_t counter = 0;
    for (size_t i = 0; i < 8; ++i) counter |= uint64_t(bytes[i]) << (8 * i);
    return counter;
}

void Expand(uint8_t* key, uint8_t* prefix, const uint8_t* master, bool fromServer, uint8_t channel) {
    SecretBytes<48> material;
    const char* domain = fromServer ? ServerDomain : ClientDomain;
    const size_t domainSize = fromServer ? sizeof(ServerDomain) - 1 : sizeof(ClientDomain) - 1;
    crypto_blake2b_ctx ctx;
    crypto_blake2b_keyed_init(&ctx, material.bytes.size(), master, 32);
    crypto_blake2b_update(&ctx, reinterpret_cast<const uint8_t*>(domain), domainSize);
    crypto_blake2b_update(&ctx, &channel, 1);
    crypto_blake2b_final(&ctx, material.bytes.data());
    crypto_wipe(&ctx, sizeof(ctx));
    std::copy_n(material.bytes.data(), 32, key);
    std::copy_n(material.bytes.data() + 32, 16, prefix);
}

void EmptyOutput(std::vector<uint8_t>& plain) {
    if (!plain.empty()) crypto_wipe(plain.data(), plain.size());
    plain.clear();
}

} // namespace

SecureChannel::~SecureChannel() { Reset(); }

bool SecureChannel::Random(uint8_t* bytes, size_t length) {
    if (length > MaxRandomBytes || (length != 0 && !bytes)) return false;
    if (length == 0) return true;
    if (RandomBytes(bytes, length)) return true;
    crypto_wipe(bytes, length);
    return false;
}

SecureChannel::SecureChannel(SecureChannel&& other) noexcept { *this = std::move(other); }

SecureChannel& SecureChannel::operator=(SecureChannel&& other) noexcept {
    if (this == &other) return *this;
    Reset();
    secretKey_ = other.secretKey_;
    publicKey_ = other.publicKey_;
    salt_ = other.salt_;
    password_ = other.password_;
    passwordSize_ = other.passwordSize_;
    channels_ = other.channels_;
    server_ = other.server_;
    begun_ = other.begun_;
    ready_ = other.ready_;
    other.Reset();
    return *this;
}

void SecureChannel::Reset() noexcept {
    crypto_wipe(secretKey_.data(), secretKey_.size());
    crypto_wipe(publicKey_.data(), publicKey_.size());
    crypto_wipe(salt_.data(), salt_.size());
    crypto_wipe(password_.data(), password_.size());
    for (auto& channel : channels_) {
        crypto_wipe(channel.sendKey.data(), channel.sendKey.size());
        crypto_wipe(channel.receiveKey.data(), channel.receiveKey.size());
        crypto_wipe(channel.sendPrefix.data(), channel.sendPrefix.size());
        crypto_wipe(channel.receivePrefix.data(), channel.receivePrefix.size());
        channel.nextSend = 0;
        channel.highestReceived = 0;
        channel.receivedMask = 0;
        channel.receivedAny = false;
    }
    passwordSize_ = 0;
    server_ = false;
    begun_ = false;
    ready_ = false;
}

bool SecureChannel::Begin(bool server, const std::string& password) {
    Reset();
    if (password.size() > MaxPasswordBytes) return false;
    if (!RandomBytes(secretKey_.data(), secretKey_.size()) ||
        (server && !RandomBytes(salt_.data(), salt_.size()))) {
        Reset();
        return false;
    }
    crypto_x25519_public_key(publicKey_.data(), secretKey_.data());
    std::copy(password.begin(), password.end(), password_.begin());
    passwordSize_ = password.size();
    server_ = server;
    begun_ = true;
    return true;
}

bool SecureChannel::Establish(const std::array<uint8_t, 32>& remotePublic,
                              const std::array<uint8_t, 16>& salt) {
    if (!begun_ || ready_) return false;
    if (!CanonicalPublicKey(remotePublic) || crypto_verify32(publicKey_.data(), remotePublic.data()) == 0 ||
        (server_ && crypto_verify16(salt_.data(), salt.data()) != 0)) {
        Reset();
        return false;
    }
    SecretBytes<32> dh;
    constexpr std::array<uint8_t, 32> Zero{};
    crypto_x25519(dh.bytes.data(), secretKey_.data(), remotePublic.data());
    crypto_wipe(secretKey_.data(), secretKey_.size());
    if (crypto_verify32(dh.bytes.data(), Zero.data()) == 0) {
        Reset();
        return false;
    }
    SecretBytes<32> passwordSecret;
    if (passwordSize_ != 0) {
        void* work = std::malloc(size_t(ArgonBlocks) * 1024);
        if (!work) {
            Reset();
            return false;
        }
        const crypto_argon2_config config{CRYPTO_ARGON2_ID, ArgonBlocks, ArgonPasses, 1};
        const crypto_argon2_inputs inputs{password_.data(), salt.data(), static_cast<uint32_t>(passwordSize_), 16};
        crypto_argon2(passwordSecret.bytes.data(), static_cast<uint32_t>(passwordSecret.bytes.size()), work,
                      config, inputs, crypto_argon2_no_extras);
        // Monocypher wipes its Argon2 work area before returning.
        std::free(work);
    }
    crypto_wipe(password_.data(), password_.size());
    passwordSize_ = 0;

    SecretBytes<32> master;
    crypto_blake2b_ctx ctx;
    crypto_blake2b_keyed_init(&ctx, master.bytes.size(), passwordSecret.bytes.data(), passwordSecret.bytes.size());
    crypto_blake2b_update(&ctx, reinterpret_cast<const uint8_t*>(ExtractDomain), sizeof(ExtractDomain) - 1);
    crypto_blake2b_update(&ctx, dh.bytes.data(), dh.bytes.size());
    const auto& hostPublic = server_ ? publicKey_ : remotePublic;
    const auto& clientPublic = server_ ? remotePublic : publicKey_;
    crypto_blake2b_update(&ctx, hostPublic.data(), hostPublic.size());
    crypto_blake2b_update(&ctx, clientPublic.data(), clientPublic.size());
    crypto_blake2b_update(&ctx, salt.data(), salt.size());
    crypto_blake2b_final(&ctx, master.bytes.data());
    crypto_wipe(&ctx, sizeof(ctx));

    for (uint8_t i = 0; i < ChannelCount; ++i) {
        auto& channel = channels_[i];
        Expand(channel.sendKey.data(), channel.sendPrefix.data(), master.bytes.data(), server_, i);
        Expand(channel.receiveKey.data(), channel.receivePrefix.data(), master.bytes.data(), !server_, i);
    }
    salt_ = salt;
    begun_ = false;
    ready_ = true;
    return true;
}

std::vector<uint8_t> SecureChannel::Seal(const std::vector<uint8_t>& plain, uint8_t channel) {
    if (!ready_ || channel >= ChannelCount || plain.size() > MaxPlaintext) return {};
    auto& state = channels_[channel];
    // Never wrap a nonce counter. The caller must initiate a fresh handshake.
    if (state.nextSend == std::numeric_limits<uint64_t>::max()) return {};
    std::vector<uint8_t> packet;
    try { packet.resize(plain.size() + Overhead); } catch (const std::bad_alloc&) { return {}; }
    std::copy_n(Magic, sizeof(Magic), packet.data());
    packet[4] = Version;
    packet[5] = channel;
    StoreCounter(packet.data() + 8, state.nextSend);
    std::array<uint8_t, 24> nonce{};
    std::copy(state.sendPrefix.begin(), state.sendPrefix.end(), nonce.begin());
    StoreCounter(nonce.data() + 16, state.nextSend);
    crypto_aead_lock(packet.data() + HeaderSize, packet.data() + HeaderSize + plain.size(),
                     state.sendKey.data(), nonce.data(), packet.data(), HeaderSize, plain.data(), plain.size());
    ++state.nextSend;
    return packet;
}

bool SecureChannel::Open(const uint8_t* bytes, size_t len, uint8_t channel, std::vector<uint8_t>& plain) {
    auto fail = [&plain]() { EmptyOutput(plain); return false; };
    if (!ready_ || channel >= ChannelCount || !bytes || len < Overhead || len > MaxDatagram) return fail();
    if (!std::equal(std::begin(Magic), std::end(Magic), bytes) || bytes[4] != Version || bytes[5] != channel ||
        bytes[6] != 0 || bytes[7] != 0) return fail();
    const uint64_t counter = LoadCounter(bytes + 8);
    if (counter == std::numeric_limits<uint64_t>::max()) return fail();
    auto& state = channels_[channel];
    if (state.receivedAny && counter <= state.highestReceived) {
        const uint64_t age = state.highestReceived - counter;
        if (age >= ReplayWindowSize || (state.receivedMask & (uint64_t(1) << age))) return fail();
    }
    std::array<uint8_t, 24> nonce{};
    std::copy(state.receivePrefix.begin(), state.receivePrefix.end(), nonce.begin());
    StoreCounter(nonce.data() + 16, counter);
    SecretBytes<MaxPlaintext> decoded;
    const size_t plainSize = len - Overhead;
    if (crypto_aead_unlock(decoded.bytes.data(), bytes + HeaderSize + plainSize, state.receiveKey.data(),
                           nonce.data(), bytes, HeaderSize, bytes + HeaderSize, plainSize) != 0) return fail();
    try {
        // A temporary also permits Open(packet.data(), ..., packet).
        std::vector<uint8_t> result(decoded.bytes.begin(), decoded.bytes.begin() + plainSize);
        EmptyOutput(plain);
        plain.swap(result);
    } catch (const std::bad_alloc&) {
        return fail();
    }
    // Replay state changes only after successful authentication and output.
    if (!state.receivedAny) {
        state.highestReceived = counter;
        state.receivedMask = 1;
        state.receivedAny = true;
    } else if (counter > state.highestReceived) {
        const uint64_t advance = counter - state.highestReceived;
        state.receivedMask = advance >= ReplayWindowSize ? 1 : (state.receivedMask << advance) | 1;
        state.highestReceived = counter;
    } else {
        state.receivedMask |= uint64_t(1) << (state.highestReceived - counter);
    }
    return true;
}

} // namespace KartNet
