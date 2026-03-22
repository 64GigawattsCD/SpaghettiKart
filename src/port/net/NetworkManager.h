#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct _ENetHost;
struct _ENetPeer;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace Network {

struct LanServerInfo {
    std::string SessionName;
    std::string Address;
    uint16_t GamePort = 0;
    bool IsSelf = false;
    uint64_t LastSeenMs = 0;
};

class NetworkManager {
  public:
    static NetworkManager& Instance();

    bool Initialize();
    void Shutdown();

    bool StartLanHost(const std::string& sessionName, uint16_t gamePort = 54777, uint16_t discoveryPort = 54778);
    void StopLanHost();
    bool IsHostingLan() const;

    void RefreshLanServers();
    std::vector<LanServerInfo> GetLanServers();

    bool JoinLanServer(const std::string& host, uint16_t port);
    void Disconnect();
    bool IsConnected() const;

    std::string GetStatusText() const;
    std::string GetSessionName() const;
    uint16_t GetGamePort() const;

  private:
    NetworkManager() = default;
    ~NetworkManager() = default;
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    void Run();
    void SetStatusText(const std::string& text);

    void EnsureDiscoverySocket();
    void DestroyDiscoverySocket();
    void PumpDiscovery();
    void BroadcastLanQuery();
    void BroadcastLanAnnouncement();
    void HandleDiscoveryPacket(const std::vector<uint8_t>& packet, const std::string& fromHost, uint16_t fromPort);
    void UpsertLanServer(const LanServerInfo& server);
    void PruneLanServers(uint64_t nowMs);

    void EnsureServerHost();
    void DestroyServerHost();
    void EnsureClientHost();
    void DestroyClientHost();
    void PumpEnetHosts();
    uint64_t GetNowMs() const;

    mutable std::mutex mMutex;
    std::thread mThread;
    bool mRunning = false;
    bool mInitialized = false;

    bool mHostingLan = false;
    bool mPendingLanRefresh = false;
    bool mConnected = false;

    std::string mSessionName = "Spaghetti Kart LAN";
    std::string mStatusText = "Networking offline";
    uint16_t mGamePort = 54777;
    uint16_t mDiscoveryPort = 54778;
    uint64_t mLastBroadcastMs = 0;

    std::unordered_map<std::string, LanServerInfo> mLanServers;

    ENetHost* mServerHost = nullptr;
    ENetHost* mClientHost = nullptr;
    ENetPeer* mServerPeer = nullptr;
    std::intptr_t mDiscoverySocket = -1;
};

} // namespace Network
