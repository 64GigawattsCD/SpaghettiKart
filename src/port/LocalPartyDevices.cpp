#include "LocalPartyDevices.h"
#include "net/NetRace.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"
#include "ship/controller/controldevice/controller/mapping/mouse/WheelHandler.h"
#include "libultraship/controller/controldevice/controller/Controller.h"
#include <libultraship.h>
#include <SDL2/SDL.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>

extern "C" {
#include "main.h"
#include "defines.h"
}

namespace KartNet {
namespace {
PartyDevices::Assignments assignments;
std::unordered_map<int32_t, bool> startHeld;
std::array<std::shared_ptr<LUS::Controller>, 4> transient;
std::array<std::vector<std::shared_ptr<Ship::ControllerMapping>>, 4> sources;
std::array<int32_t, 4> lastInstance{{-1, -1, -1, -1}};

class DeviceFilter {
    std::shared_ptr<Ship::ConnectedPhysicalDeviceManager> manager;
    std::array<std::unordered_set<int32_t>, 4> saved;
    std::array<bool, 4> changed{};
  public:
    explicit DeviceFilter(std::shared_ptr<Ship::ConnectedPhysicalDeviceManager> devices) : manager(std::move(devices)) {}
    void Only(int port, int32_t instance, const std::vector<int32_t>& connected) {
        if (!changed[port]) { saved[port] = manager->GetIgnoredInstanceIdsForPort(uint8_t(port)); changed[port] = true; }
        for (auto id : connected) {
            if (id == instance) manager->UnignoreInstanceIdForPort(uint8_t(port), id);
            else manager->IgnoreInstanceIdForPort(uint8_t(port), id);
        }
    }
    ~DeviceFilter() {
        for (int port = 0; port < 4; ++port) if (changed[port]) {
            const auto current = manager->GetIgnoredInstanceIdsForPort(uint8_t(port));
            for (auto id : current) if (!saved[port].count(id)) manager->UnignoreInstanceIdForPort(uint8_t(port), id);
            for (auto id : saved[port]) manager->IgnoreInstanceIdForPort(uint8_t(port), id);
        }
    }
};

std::string DeviceKey(SDL_GameController* device) {
    char guid[33]{};
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(device)), guid, sizeof(guid));
    std::string result(guid);
#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (const char* serial = SDL_GameControllerGetSerial(device)) result += std::string(":") + serial;
#endif
    return result;
}
bool Active(SDL_GameController* device) {
    for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button)
        if (SDL_GameControllerGetButton(device, SDL_GameControllerButton(button))) return true;
    for (auto axis : {SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY})
        if (std::abs(int(SDL_GameControllerGetAxis(device, axis))) > 16000) return true;
    return SDL_GameControllerGetAxis(device, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000 ||
           SDL_GameControllerGetAxis(device, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;
}
bool Joined(int port) {
    NetPartyInfo info{}; return net_party_info(port, &info) && info.joined;
}

std::shared_ptr<LUS::Controller> BorrowSDL(int port, const std::shared_ptr<Ship::Controller>& primary) {
    const auto mappings = primary->GetAllMappings();
    if (transient[port] && sources[port] == mappings) return transient[port];
    std::vector<CONTROLLERBUTTONS_T> masks;
    for (const auto& entry : primary->GetAllButtons()) masks.push_back(entry.first);
    auto controller = std::make_shared<LUS::Controller>(uint8_t(port), masks);
    for (const auto& [mask, button] : primary->GetAllButtons())
        for (const auto& [id, mapping] : button->GetAllButtonMappings())
            if (mapping->GetPhysicalDeviceType() == Ship::SDLGamepad) controller->GetButton(mask)->AddButtonMapping(mapping);
    auto copyStick = [](const auto& source, const auto& target) {
        for (const auto& [direction, mappings] : source->GetAllAxisDirectionMappings())
            for (const auto& [id, mapping] : mappings)
                if (mapping->GetPhysicalDeviceType() == Ship::SDLGamepad) target->AddAxisDirectionMapping(direction, mapping);
    };
    copyStick(primary->GetLeftStick(), controller->GetLeftStick());
    copyStick(primary->GetRightStick(), controller->GetRightStick());
    sources[port] = mappings; transient[port] = controller; return controller;
}
}

bool ReadLocalPartyPads(void* output) {
    if (!output || (gGamestate == RACING && !net_active())) return false;
    auto context = Ship::Context::GetRawInstance();
    if (!context) return false;
    auto deck = context->GetControlDeck();
    if (!deck || !deck->GetControllerByPort(0)) return false;
    auto devices = deck->GetConnectedPhysicalDeviceManager();
    SDL_PumpEvents(); SDL_GameControllerUpdate();
    Ship::WheelHandler::GetInstance()->Update();
    auto* pads = static_cast<OSContPad*>(output);
    std::fill_n(pads, 4, OSContPad{});
    if (deck->GamepadGameInputBlocked() && deck->KeyboardGameInputBlocked() && deck->MouseGameInputBlocked()) return true;

    auto names = devices->GetConnectedSDLGamepadNames();
    std::vector<int32_t> connected;
    for (const auto& [id, name] : names)
        if (auto* pad = SDL_GameControllerFromInstanceID(id); pad && SDL_GameControllerGetAttached(pad)) connected.push_back(id);
    std::sort(connected.begin(), connected.end());
    std::array<bool, 4> joined{};
    for (int port = 0; port < 4; ++port) joined[port] = Joined(port);
    assignments.Reconcile(connected, joined);
    for (auto it = startHeld.begin(); it != startHeld.end();) {
        if (std::find(connected.begin(), connected.end(), it->first) == connected.end()) it = startHeld.erase(it);
        else ++it;
    }
    for (auto id : connected) {
        auto* device = SDL_GameControllerFromInstanceID(id);
        const bool start = SDL_GameControllerGetButton(device, SDL_CONTROLLER_BUTTON_START) != 0;
        const bool edge = start && !startHeld[id]; startHeld[id] = start;
        int port = assignments.Find(id);
        if (port >= 0) continue;
        if (deck->GamepadGameInputBlocked()) continue;
        const auto key = DeviceKey(device);
        const int reclaim = assignments.Reclaim(key);
        if (!edge && !(Active(device) && assignments.seats[0].instance < 0 &&
            (assignments.seats[0].key.empty() || reclaim == 0))) continue;
        std::vector<int> candidates;
        if (reclaim >= 0) candidates.push_back(reclaim);
        for (int candidate = 0; candidate < 4; ++candidate)
            if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) candidates.push_back(candidate);
        for (int candidate : candidates) {
            if (assignments.seats[candidate].instance >= 0 || (!edge && candidate != 0)) continue;
            if (!Joined(candidate)) net_party_join(candidate);
            if (Joined(candidate) && assignments.Claim(candidate, id, key, names[id])) break;
        }
    }
    for (int port = 0; port < 4; ++port) {
        auto controller = deck->GetControllerByPort(uint8_t(port));
        if (!controller) continue;
        if (lastInstance[port] != assignments.seats[port].instance) {
            transient[port].reset(); sources[port].clear();
            lastInstance[port] = assignments.seats[port].instance;
        }
        const bool borrowed = port != 0 && !controller->HasConfig();
        if (borrowed) controller = BorrowSDL(port, deck->GetControllerByPort(0));
        DeviceFilter filters(devices);
        filters.Only(port, assignments.seats[port].instance, connected);
        if (borrowed) filters.Only(0, assignments.seats[port].instance, connected);
        controller->ReadToPad(&pads[port]);
        auto* device = SDL_GameControllerFromInstanceID(assignments.seats[port].instance);
        const bool start = device && SDL_GameControllerGetButton(device, SDL_CONTROLLER_BUTTON_START);
        if (assignments.ConsumeStart(port, start, int(context->GetConsoleVariables()->GetInteger("gSimulatedInputLag", 0)))) {
            pads[port].button &= ~uint32_t(START_BUTTON);
            pads[port].button_value[12] = 0;
        }
    }
    return true;
}
const char* LocalPartyDeviceName(int port) {
    return port >= 0 && port < 4 ? assignments.seats[port].name.c_str() : "";
}
int LocalPartyDeviceInstance(int port) {
    return port >= 0 && port < 4 ? assignments.seats[port].instance : -1;
}
}
