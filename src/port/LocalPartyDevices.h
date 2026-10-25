#pragma once

#ifdef __cplusplus
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace KartNet {
namespace PartyDevices {
struct Seat {
    int32_t instance = -1;
    std::string key, name;
    bool consumeStart = false;
    int releasedReads = 0;
};
class Assignments {
  public:
    std::array<Seat, 4> seats;
    int Find(int32_t instance) const {
        if (instance < 0) return -1;
        for (int i = 0; i < 4; ++i) if (seats[i].instance == instance) return i;
        return -1;
    }
    int Reclaim(const std::string& key) const {
        for (int i = 0; i < 4; ++i)
            if (seats[i].instance < 0 && !key.empty() && seats[i].key == key) return i;
        return -1;
    }
    bool Claim(int slot, int32_t instance, const std::string& key, const std::string& name) {
        if (slot < 0 || slot >= 4 || instance < 0 || Find(instance) >= 0 || seats[slot].instance >= 0) return false;
        seats[slot] = {instance, key, name, true, 0}; return true;
    }
    bool ConsumeStart(int slot, bool held, int inputLag) {
        auto& seat = seats[slot];
        if (!seat.consumeStart) return false;
        seat.releasedReads = held ? 0 : seat.releasedReads+1;
        if (!held && seat.releasedReads > std::clamp(inputLag, 0, 5)) seat.consumeStart = false;
        return true;
    }
    void Reconcile(const std::vector<int32_t>& connected, const std::array<bool, 4>& joined) {
        for (int i = 0; i < 4; ++i) {
            if (!joined[i]) seats[i] = {};
            else if (std::find(connected.begin(), connected.end(), seats[i].instance) == connected.end()) {
                seats[i].instance = -1; seats[i].consumeStart = false;
            }
        }
    }
};
}
bool ReadLocalPartyPads(void* pads);
const char* LocalPartyDeviceName(int port);
int LocalPartyDeviceInstance(int port);
}
extern "C" {
#endif
/* Returns nonzero after a RAM-only routed read, zero for the legacy read path. */
int GameEngine_ReadPartyPads(void* pads);
const char* GameEngine_LocalPartyDeviceName(int port);
int GameEngine_LocalPartyDeviceInstance(int port);
#ifdef __cplusplus
}
#endif
