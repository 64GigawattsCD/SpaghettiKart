#include "NetworkManager.h"

#include <enet/enet.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>

namespace {

constexpr uint32_t kLanDiscoveryMagic = 0x534B4C41; // SKLA
constexpr uint16_t kLanDiscoveryVersion = 1;
constexpr size_t kLanSessionNameBytes = 64;
constexpr uint64_t kLanAnnounceIntervalMs = 1000;
constexpr uint64_t kLanServerTtlMs = 5000;

enum class DiscoveryPacketType : uint8_t {
    Query = 1,
    Announce = 2,
};

struct DiscoveryPacket {
    uint32_t Magic;
    uint16_t Version;
    uint8_t Type;
    uint16_t GamePort;
    char SessionName[kLanSessionNameBytes];
};

std::string AddressToString(const ENetAddress& address) {
    char host[64] = {};
    if (enet_address_get_host_ip(&address, host, sizeof(host)) != 0) {
        return {};
    }
    return host;
}

ENetSocket MakeBroadcastSocket(uint16_t port) {
    ENetSocket socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (socket == ENET_SOCKET_NULL) {
        return socket;
    }

    enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1);
    enet_socket_set_option(socket, ENET_SOCKOPT_BROADCAST, 1);
    enet_socket_set_option(socket, ENET_SOCKOPT_REUSEADDR, 1);

    ENetAddress bindAddress{};
    bindAddress.host = ENET_HOST_ANY;
    bindAddress.port = port;
    if (enet_socket_bind(socket, &bindAddress) != 0) {
        enet_socket_destroy(socket);
        return ENET_SOCKET_NULL;
    }

    return socket;
}

std::vector<uint8_t> SerializeDiscoveryPacket(DiscoveryPacketType type, uint16_t gamePort, const std::string& sessionName) {
    DiscoveryPacket packet{};
    packet.Magic = kLanDiscoveryMagic;
    packet.Version = kLanDiscoveryVersion;
    packet.Type = static_cast<uint8_t>(type);
    packet.GamePort = gamePort;
    std::strncpy(packet.SessionName, sessionName.c_str(), sizeof(packet.SessionName) - 1);

    const auto* bytes = reinterpret_cast<const uint8_t*>(&packet);
    return std::vector<uint8_t>(bytes, bytes + sizeof(packet));
}

bool DeserializeDiscoveryPacket(const std::vector<uint8_t>& rawPacket, DiscoveryPacket& packet) {
    if (rawPacket.size() != sizeof(DiscoveryPacket)) {
        return false;
    }

    std::memcpy(&packet, rawPacket.data(), sizeof(packet));
    return packet.Magic == kLanDiscoveryMagic && packet.Version == kLanDiscoveryVersion;
}

} // namespace

namespace Network {

NetworkManager& NetworkManager::Instance() {
    static NetworkManager manager;
    return manager;
}

bool NetworkManager::Initialize() {
    std::scoped_lock lock(mMutex);
    if (mInitialized) {
        return true;
    }

    if (enet_initialize() != 0) {
        mStatusText = "Failed to initialize ENet";
        return false;
    }

    mRunning = true;
    mInitialized = true;
    mStatusText = "Networking ready";
    mThread = std::thread(&NetworkManager::Run, this);
    return true;
}

void NetworkManager::Shutdown() {
    {
        std::scoped_lock lock(mMutex);
        if (!mInitialized) {
            return;
        }
        mRunning = false;
    }

    if (mThread.joinable()) {
        mThread.join();
    }

    {
        std::scoped_lock lock(mMutex);
        DestroyClientHost();
        DestroyServerHost();
        DestroyDiscoverySocket();
        mLanServers.clear();
        mConnected = false;
        mHostingLan = false;
        mInitialized = false;
        mStatusText = "Networking offline";
    }

    enet_deinitialize();
}

bool NetworkManager::StartLanHost(const std::string& sessionName, uint16_t gamePort, uint16_t discoveryPort) {
    std::scoped_lock lock(mMutex);
    mSessionName = sessionName.empty() ? "Spaghetti Kart LAN" : sessionName;
    mGamePort = gamePort;
    mDiscoveryPort = discoveryPort;
    mHostingLan = true;
    mPendingLanRefresh = true;
    mStatusText = "Starting LAN host...";
    return true;
}

void NetworkManager::StopLanHost() {
    std::scoped_lock lock(mMutex);
    mHostingLan = false;
    DestroyServerHost();
    SetStatusText("LAN host stopped");
}

bool NetworkManager::IsHostingLan() const {
    std::scoped_lock lock(mMutex);
    return mHostingLan;
}

void NetworkManager::RefreshLanServers() {
    std::scoped_lock lock(mMutex);
    mPendingLanRefresh = true;
}

std::vector<LanServerInfo> NetworkManager::GetLanServers() {
    std::scoped_lock lock(mMutex);
    std::vector<LanServerInfo> servers;
    servers.reserve(mLanServers.size());
    for (const auto& [key, value] : mLanServers) {
        servers.push_back(value);
    }
    std::sort(servers.begin(), servers.end(), [](const auto& left, const auto& right) {
        if (left.IsSelf != right.IsSelf) {
            return left.IsSelf;
        }
        return left.SessionName < right.SessionName;
    });
    return servers;
}

bool NetworkManager::JoinLanServer(const std::string& host, uint16_t port) {
    std::scoped_lock lock(mMutex);
    EnsureClientHost();
    if (mClientHost == nullptr) {
        SetStatusText("Unable to create LAN client");
        return false;
    }

    ENetAddress address{};
    address.port = port;
    if (enet_address_set_host(&address, host.c_str()) != 0) {
        SetStatusText("Failed to resolve LAN host");
        return false;
    }

    mServerPeer = enet_host_connect(mClientHost, &address, 2, 0);
    if (mServerPeer == nullptr) {
        SetStatusText("Failed to start LAN connection");
        return false;
    }

    SetStatusText("Connecting to " + host + ":" + std::to_string(port));
    return true;
}

void NetworkManager::Disconnect() {
    std::scoped_lock lock(mMutex);
    if (mServerPeer != nullptr) {
        enet_peer_disconnect(mServerPeer, 0);
    }
    mConnected = false;
    SetStatusText("Disconnected");
}

bool NetworkManager::IsConnected() const {
    std::scoped_lock lock(mMutex);
    return mConnected;
}

std::string NetworkManager::GetStatusText() const {
    std::scoped_lock lock(mMutex);
    return mStatusText;
}

std::string NetworkManager::GetSessionName() const {
    std::scoped_lock lock(mMutex);
    return mSessionName;
}

uint16_t NetworkManager::GetGamePort() const {
    std::scoped_lock lock(mMutex);
    return mGamePort;
}

void NetworkManager::Run() {
    while (true) {
        {
            std::scoped_lock lock(mMutex);
            if (!mRunning) {
                break;
            }

            EnsureDiscoverySocket();
            if (mHostingLan) {
                EnsureServerHost();
            }

            PumpEnetHosts();
            PumpDiscovery();

            const uint64_t nowMs = GetNowMs();
            if (mPendingLanRefresh) {
                BroadcastLanQuery();
                mPendingLanRefresh = false;
            }

            if (mHostingLan && nowMs - mLastBroadcastMs >= kLanAnnounceIntervalMs) {
                BroadcastLanAnnouncement();
                mLastBroadcastMs = nowMs;
            }

            PruneLanServers(nowMs);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void NetworkManager::SetStatusText(const std::string& text) {
    mStatusText = text;
    SPDLOG_INFO("[LAN] {}", text);
}

void NetworkManager::EnsureDiscoverySocket() {
    if (mDiscoverySocket != -1) {
        return;
    }

    const ENetSocket socket = MakeBroadcastSocket(mDiscoveryPort);
    if (socket == ENET_SOCKET_NULL) {
        SetStatusText("Failed to create LAN discovery socket");
        return;
    }

    mDiscoverySocket = static_cast<std::intptr_t>(socket);
}

void NetworkManager::DestroyDiscoverySocket() {
    if (mDiscoverySocket == -1) {
        return;
    }

    enet_socket_destroy(static_cast<ENetSocket>(mDiscoverySocket));
    mDiscoverySocket = -1;
}

void NetworkManager::PumpDiscovery() {
    if (mDiscoverySocket == -1) {
        return;
    }

    for (;;) {
        std::array<uint8_t, sizeof(DiscoveryPacket)> buffer{};
        ENetAddress from{};
        ENetBuffer enetBuffer{};
        enetBuffer.data = buffer.data();
        enetBuffer.dataLength = buffer.size();

        const int received = enet_socket_receive(static_cast<ENetSocket>(mDiscoverySocket), &from, &enetBuffer, 1);
        if (received <= 0) {
            break;
        }

        std::vector<uint8_t> packet(buffer.begin(), buffer.begin() + received);
        HandleDiscoveryPacket(packet, AddressToString(from), from.port);
    }
}

void NetworkManager::BroadcastLanQuery() {
    if (mDiscoverySocket == -1) {
        return;
    }

    const auto payload = SerializeDiscoveryPacket(DiscoveryPacketType::Query, mGamePort, mSessionName);
    ENetAddress address{};
    address.host = ENET_HOST_BROADCAST;
    address.port = mDiscoveryPort;
    ENetBuffer buffer{};
    buffer.data = const_cast<uint8_t*>(payload.data());
    buffer.dataLength = payload.size();
    enet_socket_send(static_cast<ENetSocket>(mDiscoverySocket), &address, &buffer, 1);
    SetStatusText("Searching LAN sessions...");
}

void NetworkManager::BroadcastLanAnnouncement() {
    if (mDiscoverySocket == -1) {
        return;
    }

    const auto payload = SerializeDiscoveryPacket(DiscoveryPacketType::Announce, mGamePort, mSessionName);
    ENetAddress address{};
    address.host = ENET_HOST_BROADCAST;
    address.port = mDiscoveryPort;
    ENetBuffer buffer{};
    buffer.data = const_cast<uint8_t*>(payload.data());
    buffer.dataLength = payload.size();
    enet_socket_send(static_cast<ENetSocket>(mDiscoverySocket), &address, &buffer, 1);
}

void NetworkManager::HandleDiscoveryPacket(const std::vector<uint8_t>& packet, const std::string& fromHost, uint16_t) {
    DiscoveryPacket decoded{};
    if (!DeserializeDiscoveryPacket(packet, decoded)) {
        return;
    }

    if (static_cast<DiscoveryPacketType>(decoded.Type) == DiscoveryPacketType::Query) {
        if (mHostingLan) {
            BroadcastLanAnnouncement();
        }
        return;
    }

    LanServerInfo server{};
    server.Address = fromHost;
    server.GamePort = decoded.GamePort;
    server.SessionName = decoded.SessionName;
    server.LastSeenMs = GetNowMs();
    server.IsSelf = mHostingLan && decoded.GamePort == mGamePort;
    UpsertLanServer(server);
}

void NetworkManager::UpsertLanServer(const LanServerInfo& server) {
    const std::string key = server.Address + ":" + std::to_string(server.GamePort);
    mLanServers[key] = server;
    if (!server.IsSelf) {
        SetStatusText("Found LAN session: " + server.SessionName);
    }
}

void NetworkManager::PruneLanServers(uint64_t nowMs) {
    for (auto it = mLanServers.begin(); it != mLanServers.end();) {
        if (nowMs - it->second.LastSeenMs > kLanServerTtlMs) {
            it = mLanServers.erase(it);
        } else {
            ++it;
        }
    }
}

void NetworkManager::EnsureServerHost() {
    if (mServerHost != nullptr) {
        return;
    }

    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = mGamePort;
    mServerHost = enet_host_create(&address, 8, 2, 0, 0);
    if (mServerHost == nullptr) {
        SetStatusText("Failed to create LAN host");
        return;
    }

    SetStatusText("Hosting LAN session on port " + std::to_string(mGamePort));
}

void NetworkManager::DestroyServerHost() {
    if (mServerHost == nullptr) {
        return;
    }

    enet_host_destroy(mServerHost);
    mServerHost = nullptr;
}

void NetworkManager::EnsureClientHost() {
    if (mClientHost != nullptr) {
        return;
    }

    mClientHost = enet_host_create(nullptr, 1, 2, 0, 0);
}

void NetworkManager::DestroyClientHost() {
    if (mClientHost == nullptr) {
        return;
    }

    enet_host_destroy(mClientHost);
    mClientHost = nullptr;
    mServerPeer = nullptr;
}

void NetworkManager::PumpEnetHosts() {
    if (mServerHost != nullptr) {
        ENetEvent event{};
        while (enet_host_service(mServerHost, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    SetStatusText("LAN client connected");
                    break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    std::string text(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
                    if (text == "SK_HELLO") {
                        ENetPacket* reply = enet_packet_create("SK_HELLO_ACK", 12, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 0, reply);
                    }
                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                    SetStatusText("LAN client disconnected");
                    break;
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }

    if (mClientHost != nullptr) {
        ENetEvent event{};
        while (enet_host_service(mClientHost, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    ENetPacket* hello = enet_packet_create("SK_HELLO", 8, ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, hello);
                    enet_host_flush(mClientHost);
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE: {
                    std::string text(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
                    if (text == "SK_HELLO_ACK") {
                        mConnected = true;
                        SetStatusText("Connected to LAN host");
                    }
                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                    mConnected = false;
                    mServerPeer = nullptr;
                    SetStatusText("LAN connection closed");
                    break;
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }
}

uint64_t NetworkManager::GetNowMs() const {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace Network
