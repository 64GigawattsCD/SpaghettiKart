#include "NetRace.h"
#include "game_options.h"
#include "Session.h"
#include "Discovery.h"
#include "ItemReplication.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <imgui.h>
#include <libultraship.h>
#include "port/Game.h"
extern "C" {
#include "main.h"
#include "menus.h"
#include "defines.h"
#include "spawn_players.h"
#include "player_controller.h"
#include "code_80005FD0.h"
#include "code_80057C60.h"
#include "kart_transmission.h"
#include "kart_input.h"
#include "racing/race_logic.h"
#include "racing/render_courses.h"
}

namespace {
KartNet::Session session;
KartNet::Discovery discovery;
bool initialized = false, raceActive = false, loadConsumed = false;
uint32_t sequence = 0, hostTick = 0, seenSnapshot = 0, snapshotAt = 0;
KartNet::Message previous;
std::array<int, 8> gears{};
std::array<float, 8> rpms{};
std::array<std::array<uint32_t, 9>, 8> lapTimes{};
std::array<Controller, 8> remoteControllers{};
std::array<int, 8> finishOrder{};
int finishCount = 0;
float firstFinishTime = -1.0f;
uint32_t loadedEpoch = ~0u;
int command = 0;
char address[64] = "192.168.1.1";
int port = 54777, character = 0;
KartNet::Party party{{0, 0, KART_TRANSMISSION_AUTOMATIC, true}};
KartNet::RaceSettings settings;
char password[64]{};
// Stock item actors, inventories and balloons use the authoritative world sidecar.
constexpr bool OnlineItemWorldEnabled = true;

int racerCount() {
    return session.settings.cpuFill ? 8 : int(session.roster.size());
}
const KartNet::Identity* identityForSlot(int slot) {
    for (const auto& identity : session.roster)
        if (session.EngineSlot(identity.participant) == slot) return &identity;
    return nullptr;
}
int globalRacerForSlot(int slot) {
    return int(session.ParticipantForSlot(slot)) - 1;
}
int slotForGlobalRacer(int racer) {
    return session.EngineSlot(uint16_t(racer + 1));
}
void resetRace() {
    KartNet::ResetItemWorld();
    sequence = hostTick = seenSnapshot = 0;
    lapTimes = {}; previous = {}; remoteControllers = {}; gears = {}; rpms = {};
    finishOrder.fill(-1); finishCount = 0; firstFinishTime = -1.0f;
}
bool startRulesAvailable(const KartNet::RaceSettings& rules) {
    if (rules.mode != 2) return true;
    if (rules.scope != 0 && !OnlineItemWorldEnabled) { session.error = "Online Battle awaits balloon and item world replication"; return false; }
    if ((rules.scope == 0 ? party.size() : session.roster.size()) < 2) { session.error = "Battle needs at least two drivers"; return false; }
    if (rules.balloons > 3) { session.error = "Battle supports one to three balloons"; return false; }
    if (rules.cpuFill) { session.error = "Battle CPU fill is not yet available for sessions"; return false; }
    if (rules.course < 16 || rules.course > 19) { session.error = "Choose a Battle arena"; return false; }
    return true;
}
uint64_t contentHash() {
    static uint64_t cached = 0;
    if (cached) return cached;
    // Compare the base game archive, not platform-specific archive packaging.
    std::ifstream in("mk64.o2r", std::ios::binary);
    if (!in) return 0;
    uint64_t hash = 14695981039346656037ull;
    std::array<char, 65536> bytes;
    while (in) {
        in.read(bytes.data(), bytes.size());
        for (std::streamsize i = 0; i < in.gcount(); ++i) { hash ^= uint8_t(bytes[i]); hash *= 1099511628211ull; }
    }
    return cached = hash;
}
void beginSession(bool host) {
    if (gGamestate == RACING) return;
    raceActive = loadConsumed = false; loadedEpoch = ~0u; resetRace();
    uint64_t hash = contentHash();
    if (!hash) { session.error = "Base game archive was not found"; session.phase = KartNet::Phase::Error; return; }
    if (host) session.Host(uint16_t(port), hash, party, settings, password, CVarGetString("gNet.BindAddress", "0.0.0.0"));
    else session.Join(address, uint16_t(port), hash, party, password);
    CVarSetInteger("gNet.Window", 1);
}
void copyInput(const KartNet::Input& input, OSContPad& pad) {
    pad = {}; pad.button = input.buttons; pad.stick_x = input.x; pad.stick_y = input.y;
    for (int bit = 0; bit < CONTROLLER_BUTTON_VALUE_COUNT; ++bit)
        pad.button_value[bit] = (input.buttons & (1u << bit)) ? 1.0f : 0.0f;
    pad.button_value[15] = input.throttle / 255.0f; pad.button_value[14] = input.brake / 255.0f;
}
void updateRemoteController(const KartNet::Input& input, Controller& controller) {
    OSContPad pad{}; copyInput(input, pad);
    controller.rawStickX = pad.stick_x; controller.rawStickY = pad.stick_y;
    controller.leftTrigger = input.brake / 255.0f;
    controller.rightTrigger = input.throttle / 255.0f;
    std::copy_n(pad.button_value, CONTROLLER_BUTTON_VALUE_COUNT, controller.buttonValue);
    controller.buttonPressed = pad.button & ~controller.button;
    controller.buttonDepressed = controller.button & ~pad.button;
    controller.button = pad.button;
    uint16_t stick = (input.x < -50 ? L_JPAD : 0) | (input.x > 50 ? R_JPAD : 0) |
        (input.y < -50 ? D_JPAD : 0) | (input.y > 50 ? U_JPAD : 0);
    controller.stickPressed = stick & ~controller.stickDirection;
    controller.stickDepressed = controller.stickDirection & ~stick;
    controller.stickDirection = stick;
}
KartNet::Message captureState() {
    KartNet::Message m; m.tick = ++hostTick; m.time = gCourseTimer; m.round = session.RaceIndex(); m.karts.clear();
    for (int i = 0; i < racerCount(); ++i) {
        const auto& p = gPlayers[i];
        m.karts.emplace_back(); auto& k = m.karts.back();
        k.participant = globalRacerForSlot(i) + 1;
        k.type = p.type; k.effects = p.effects;
        std::copy_n(p.pos, 3, k.pos.begin()); std::copy_n(p.velocity, 3, k.velocity.begin());
        std::copy_n(p.oldPos, 3, k.oldPos.begin());
        std::copy_n(p.rotation, 3, k.rotation.begin());
        for (int a = 0; a < 3; ++a) for (int b = 0; b < 3; ++b) k.orientation[a*3+b] = p.orientationMatrix[a][b];
        for (int tyre = 0; tyre < 4; ++tyre) for (int a = 0; a < 3; ++a) k.tyres[tyre*3+a] = p.tyres[tyre].pos[a];
        k.ground = {p.unk_1F8, p.unk_1FC, p.unk_230, p.unk_23C};
        k.speed = p.speed; k.size = p.size; k.rpm = kart_transmission_get_engine_rpm(&p, i);
        k.gear = int8_t(kart_transmission_get_gear(i)); k.steering = p.unk_07C;
        k.driftYaw = p.unk_0C0; k.slope = p.slopeAccel;
        k.lap = uint8_t(std::clamp(gLapCountByPlayerId[i], 0, int(session.settings.laps)));
        k.rank = uint8_t(std::clamp(int(p.currentRank), 0, racerCount()-1));
        for (int lap = 0; lap < k.lap; ++lap)
            if (!lapTimes[i][lap]) lapTimes[i][lap] = uint32_t(gCourseTimer * 100);
        std::copy_n(lapTimes[i].begin(), std::min(k.lapTimes.size(), lapTimes[i].size()), k.lapTimes.begin());
    }
    return m;
}
void applyState() {
    if (!session.receivedSnapshots) return;
    const auto& m = session.snapshot;
    if (seenSnapshot != session.receivedSnapshots) {
        if (!seenSnapshot) previous = m;
        seenSnapshot = session.receivedSnapshots; snapshotAt = enet_time_get();
    }
    float alpha = std::min(1.0f, (enet_time_get()-snapshotAt) / 33.333f);
    gCourseTimer = m.time;
    for (const auto& k : m.karts) {
        const auto found = std::find_if(previous.karts.begin(), previous.karts.end(),
            [&](const KartNet::Kart& old) { return old.participant == k.participant; });
        const auto& old = found == previous.karts.end() ? k : *found;
        int slot = slotForGlobalRacer(int(k.participant)-1);
        if (slot < 0 || slot >= 8 || k.rank >= 8) continue;
        auto& p = gPlayers[slot];
        for (int a = 0; a < 3; ++a) {
            p.oldPos[a] = old.oldPos[a] + (k.oldPos[a]-old.oldPos[a])*alpha;
            p.pos[a] = old.pos[a] + (k.pos[a]-old.pos[a])*alpha;
            p.velocity[a] = k.velocity[a];
            p.rotation[a] = int16_t(old.rotation[a] + int16_t(k.rotation[a]-old.rotation[a])*alpha);
            for (int b = 0; b < 3; ++b) p.orientationMatrix[a][b] = k.orientation[a*3+b];
        }
        p.type = k.type; p.effects = k.effects; p.speed = p.currentSpeed = k.speed;
        for (int tyre = 0; tyre < 4; ++tyre) for (int a = 0; a < 3; ++a)
            p.tyres[tyre].pos[a] = old.tyres[tyre*3+a] + (k.tyres[tyre*3+a]-old.tyres[tyre*3+a])*alpha;
        p.unk_1F8 = k.ground[0]; p.unk_1FC = k.ground[1]; p.unk_230 = k.ground[2]; p.unk_23C = k.ground[3];
        p.unk_07C = k.steering; p.unk_0C0 = k.driftYaw; p.slopeAccel = k.slope;
        p.size = k.size;
        p.lapCount = k.lap; gLapCountByPlayerId[slot] = k.lap;
        p.currentRank = k.rank; gGPCurrentRaceRankByPlayerId[slot] = k.rank;
        gGPCurrentRacePlayerIdByRank[k.rank] = gPlayerPositionLUT[k.rank] = slot;
        gears[slot] = k.gear; rpms[slot] = k.rpm;
        if (slot < int(session.LocalCount())) for (int lap = 0; lap < 3; ++lap) {
            playerHUD[slot].lapCompletionTimes[lap] = k.lapTimes[lap];
            playerHUD[slot].lapDurations[lap] = k.lapTimes[lap] ? k.lapTimes[lap] - (lap ? k.lapTimes[lap-1] : 0) : 0;
        }
    }
}
}

extern "C" int net_active(void) { return session.Active(); }
extern "C" int net_race_active(void) { return raceActive; }
extern "C" int net_is_host(void) { return session.IsHost(); }
extern "C" int net_running(void) { return session.phase == KartNet::Phase::Racing; }
extern "C" int net_gear(int player) { return gears[std::clamp(player, 0, 7)]; }
extern "C" float net_rpm(int player) { return rpms[std::clamp(player, 0, 7)]; }
extern "C" int net_total_laps(void) { return session.settings.laps; }
extern "C" int net_course_index(void) { return session.settings.CourseIndex(); }
extern "C" int net_items_enabled(void) { return (session.LocalOnly() || OnlineItemWorldEnabled) && session.settings.items != 2; }
extern "C" unsigned int net_capabilities(void) { return OnlineItemWorldEnabled ? NET_CAP_ONLINE_ITEMS | NET_CAP_ONLINE_BATTLE : 0; }
extern "C" int net_world_paused(void) { return raceActive && session.LocalOnly() && game_options_is_live_network(); }
extern "C" int net_item_mode(void) { return session.settings.items; }
extern "C" Controller* net_controller_for_racer(int slot) {
    if (!raceActive || !session.IsHost() || slot < 0 || slot >= 8) return nullptr;
    const auto* identity = identityForSlot(slot);
    if (!identity) return nullptr;
    return identity->connection == session.LocalConnection() ? &gControllers[slot] : &remoteControllers[slot];
}

extern "C" void net_menu_state(NetMenuState* state) {
    if (!state) return;
    *state = {};
    state->active = session.Active(); state->inRace = raceActive;
    state->canStart = session.CanStart();
    state->character = character; state->port = port;
    std::snprintf(state->address, sizeof(state->address), "%s", address);
    const auto& rules = session.Active() ? session.settings : settings;
    state->phase = int(session.phase); state->host = session.IsHost(); state->localOnly = session.Active() ? session.LocalOnly() : rules.scope == 0;
    state->scope = rules.scope; state->mode = rules.mode; state->maxPlayers = rules.maxPlayers;
    state->cup = rules.cup; state->course = rules.course; state->cc = rules.cc; state->laps = rules.laps;
    state->cpuFill = rules.cpuFill; state->mirror = rules.mirror; state->items = rules.items; state->balloons = rules.balloons;
    state->localCount = session.Active() ? session.LocalCount() : int(party.size()); state->playerCount = int(session.roster.size());
    state->ready = std::all_of(party.begin(), party.end(), [](const auto& p) { return p.ready; });
    state->raceIndex = session.RaceIndex(); state->cupComplete = session.CupComplete(); state->passwordSet = password[0] != 0;
    std::snprintf(state->name, sizeof(state->name), "%s", rules.name.c_str());
    if (!session.error.empty()) std::snprintf(state->status, sizeof(state->status), "%s", session.error.c_str());
    else if (!session.Active() && !discovery.Error().empty()) std::snprintf(state->status, sizeof(state->status), "%s", discovery.Error().c_str());
    else std::snprintf(state->status, sizeof(state->status), "%s  %u MS", KartNet::PhaseName(session.phase), session.Rtt());
}

extern "C" void net_menu_adjust(int field, int direction) {
    const bool lobbyHost = session.Active() && session.IsHost() && session.phase == KartNet::Phase::Lobby;
    if ((session.Active() && !lobbyHost) || (gGamestate == RACING && !lobbyHost)) return;
    if (lobbyHost && field < NET_SETTING_SCOPE) return;
    if (lobbyHost) settings = session.settings;
    const int step = direction ? std::clamp(direction, -10, 10) : 1;
    if (field == 0) { net_party_adjust(0, 0, step); }
    else if (field == 1) { port = std::clamp(port + step, 1, 65535); CVarSetInteger("gNet.Port", port); }
    else if (field >= 2 && field <= 5) {
        int octets[4]{};
        std::sscanf(address, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]);
        octets[field - 2] = (std::clamp(octets[field - 2], 0, 255) + step + 256) % 256;
        std::snprintf(address, sizeof(address), "%d.%d.%d.%d", octets[0], octets[1], octets[2], octets[3]);
        CVarSetString("gNet.Address", address);
    }
    auto cycle = [step](uint8_t& field, int lo, int hi) { field = uint8_t(lo + (int(field)-lo+step%(hi-lo+1)+(hi-lo+1))%(hi-lo+1)); };
    switch (field) {
        case NET_SETTING_SCOPE: cycle(settings.scope, 0, 2); break;
        case NET_SETTING_MODE:
            cycle(settings.mode, 0, 2);
            if (settings.mode == 2) { settings.mirror = false; settings.cpuFill = false; }
            if (settings.mode == 2 && settings.course < 16) settings.course = 16;
            if (settings.mode != 2 && settings.course >= 16) settings.course = 0;
            break;
        case NET_SETTING_MAX_PLAYERS: cycle(settings.maxPlayers, int(party.size()), 8); break;
        case NET_SETTING_CUP: cycle(settings.cup, 0, 3); break;
        case NET_SETTING_COURSE: cycle(settings.course, settings.mode == 2 ? 16 : 0, settings.mode == 2 ? 19 : 15); break;
        case NET_SETTING_CC: cycle(settings.cc, 0, 3); break;
        case NET_SETTING_LAPS: cycle(settings.laps, 1, 9); break;
        case NET_SETTING_CPU_FILL: settings.cpuFill = settings.mode != 2 && !settings.cpuFill; break;
        case NET_SETTING_MIRROR: settings.mirror = settings.mode != 2 && !settings.mirror; break;
        case NET_SETTING_ITEMS: cycle(settings.items, 0, 2); break;
        case NET_SETTING_BALLOONS: cycle(settings.balloons, 1, 3); break;
    }
    if (lobbyHost) session.SetSettings(settings);
}

extern "C" void net_menu_command(int action) {
    if ((action == NET_HOST || action == NET_JOIN || action == NET_LOCAL) && !session.Active() && gGamestate != RACING) command = action;
    else if (action == NET_START && session.CanStart()) command = action;
    else if (action == NET_LEAVE || action == NET_RETURN_LOBBY || action == NET_READY || action == NET_REFRESH) command = action;
}

extern "C" int net_party_info(int controller, NetPartyInfo* info) {
    if (!info || controller < 0 || controller >= 4) return 0;
    *info = {}; info->controller = controller;
    for (const auto& p : party) if (p.controller == controller) {
        *info = {1, controller, p.character, p.transmission, p.ready}; return 1;
    }
    return 0;
}
extern "C" void net_party_join(int controller) {
    if ((session.Active() && session.phase != KartNet::Phase::Lobby) || controller < 0 || controller >= 4 || party.size() >= 4) return;
    NetPartyInfo info{}; if (net_party_info(controller, &info)) return;
    int selected = 0;
    while (std::any_of(party.begin(), party.end(), [selected](const auto& p) { return p.character == selected; })) ++selected;
    auto oldParty = party;
    party.push_back({uint8_t(controller), uint8_t(selected), KART_TRANSMISSION_AUTOMATIC, false});
    std::sort(party.begin(), party.end(), [](const auto& a, const auto& b) { return a.controller < b.controller; });
    settings.maxPlayers = std::max(settings.maxPlayers, uint8_t(party.size()));
    if (session.Active() && !session.UpdateParty(party)) party = oldParty;
}
extern "C" void net_party_leave(int controller) {
    if ((session.Active() && session.phase != KartNet::Phase::Lobby) || controller == 0) return;
    auto oldParty = party;
    party.erase(std::remove_if(party.begin(), party.end(), [controller](const auto& p) { return p.controller == controller; }), party.end());
    if (session.Active() && !session.UpdateParty(party)) party = oldParty;
}
extern "C" void net_party_ready(int controller, int ready) {
    for (size_t i = 0; i < party.size(); ++i) if (party[i].controller == controller) {
        party[i].ready = ready != 0;
        if (session.Active()) session.SetPlayerReady(uint8_t(i), ready != 0);
    }
}
extern "C" void net_party_adjust(int controller, int field, int direction) {
    if (field == 2) { NetPartyInfo info{}; if (net_party_info(controller, &info)) net_party_ready(controller, !info.ready); return; }
    if (session.Active() && session.phase != KartNet::Phase::Lobby) return;
    const auto oldParty = party;
    for (auto& p : party) if (p.controller == controller) {
        if (field == 0) {
            const int step = direction < 0 ? -1 : 1;
            int selected = p.character;
            do { selected = (selected + step + 8) % 8; }
            while (std::any_of(party.begin(), party.end(), [&](const auto& other) { return other.controller != controller && other.character == selected; }));
            p.character = uint8_t(selected);
            if (controller == 0) { character = selected; CVarSetInteger("gNet.Character", character); }
        } else if (field == 1) p.transmission = !p.transmission;
        p.ready = false;
    }
    if (session.Active() && !session.UpdateParty(party)) party = oldParty;
}
extern "C" int net_roster_info(int index, NetRosterInfo* info) {
    if (!info || index < 0 || index >= int(session.roster.size())) return 0;
    const auto& p = session.roster[index]; const int slot = session.EngineSlot(p.participant);
    const bool local = p.connection == session.LocalConnection();
    const int controller = local && p.controller < party.size() ? party[p.controller].controller : p.controller;
    *info = {p.participant, p.connection, p.racer, controller, p.character, p.transmission,
        p.ready, p.connected, p.connection == session.LocalConnection(),
        raceActive && slot >= 0 ? int(gPlayers[slot].currentRank) : index, session.standings[p.racer],
        raceActive && slot >= 0 && (session.settings.mode == 2 ?
            session.phase == KartNet::Phase::Finished && p.connected : gLapCountByPlayerId[slot] >= session.settings.laps)};
    return 1;
}
extern "C" void net_menu_text(int field, const char* text) {
    if (!text || session.Active()) return;
    if (field == NET_TEXT_ADDRESS) std::snprintf(address, sizeof(address), "%s", text);
    else if (field == NET_TEXT_NAME) {
        std::string name(text);
        if (!name.empty() && name.size() <= 47 && std::all_of(name.begin(), name.end(), [](unsigned char c) { return c >= 32 && c <= 126; })) settings.name = name;
    }
    else if (field == NET_TEXT_PASSWORD) std::snprintf(password, sizeof(password), "%s", text);
    else if (field == NET_TEXT_PORT) {
        int parsed = 0; const auto end = text + std::strlen(text);
        const auto result = std::from_chars(text, end, parsed);
        if (result.ec == std::errc{} && result.ptr == end && parsed >= 1 && parsed <= 65535) {
            port = parsed; CVarSetInteger("gNet.Port", port);
        }
    }
}
extern "C" void net_menu_get_text(int field, char* text, int capacity) {
    if (!text || capacity <= 0) return;
    if (field == NET_TEXT_PORT) { std::snprintf(text, size_t(capacity), "%d", port); return; }
    const char* source = field == NET_TEXT_ADDRESS ? address : field == NET_TEXT_NAME ? settings.name.c_str() : password;
    std::snprintf(text, size_t(capacity), "%s", source);
}
extern "C" int net_browser_count(void) { return int(discovery.Servers().size()); }
extern "C" int net_browser_info(int index, NetServerInfo* info) {
    if (!info || index < 0 || index >= int(discovery.Servers().size())) return 0;
    const auto& server = discovery.Servers()[index];
    *info = {};
    std::snprintf(info->name, sizeof(info->name), "%s", server.name.c_str());
    std::snprintf(info->address, sizeof(info->address), "%s", server.address.c_str());
    info->port = server.port; info->mode = server.mode; info->cup = server.cup; info->course = server.course;
    info->players = server.players; info->maxPlayers = server.capacity; info->password = server.locked;
    info->compatible = server.compatible; info->ping = int(server.ping); info->phase = int(server.phase);
    return 1;
}
extern "C" void net_browser_join(int index) {
    NetServerInfo server{};
    if (session.Active() || !net_browser_info(index, &server) || !server.compatible ||
        server.phase != int(KartNet::Phase::Lobby) || server.players + party.size() > server.maxPlayers) return;
    std::snprintf(address, sizeof(address), "%s", server.address); port = server.port;
    command = NET_JOIN;
}

extern "C" void net_read_inputs(void) {
    if (!initialized) {
        initialized = true;
        port = std::clamp(CVarGetInteger("gNet.Port", 54777), 1, 65535);
        character = std::clamp(CVarGetInteger("gNet.Character", 0), 0, 7);
        party[0].character = uint8_t(character);
        const char* endpoint = std::getenv("SK_NET_JOIN");
        std::snprintf(address, sizeof(address), "%s", endpoint ? endpoint : CVarGetString("gNet.Address", "192.168.1.1"));
        command = endpoint ? 2 : CVarGetInteger("gNet.Mode", 0);
        CVarSetInteger("gNet.Mode", 0);
    }
    if (command == NET_HOST || command == NET_JOIN) beginSession(command == NET_HOST);
    if (command == NET_LOCAL && gGamestate != RACING) {
        raceActive = loadConsumed = false; loadedEpoch = ~0u; resetRace();
        settings.scope = 0;
        for (auto& p : party) p.ready = true;
        if (startRulesAvailable(settings) && session.Local(contentHash(), party, settings)) { if (session.Start()) loadConsumed = false; }
    }
    if (command == NET_START) {
        if (startRulesAvailable(session.settings) && session.Start()) loadConsumed = false;
    }
    if (command == NET_READY) {
        bool ready = !std::all_of(party.begin(), party.end(), [](const auto& p) { return p.ready; });
        for (auto& p : party) p.ready = ready;
        session.SetReady(ready);
    }
    if (command == NET_REFRESH) discovery.Refresh(contentHash(), uint8_t(party.size()));
    if (command == NET_RETURN_LOBBY && session.IsHost() && session.ReturnToLobby()) {
        raceActive = loadConsumed = false; loadedEpoch = ~0u;
        if (gGamestate == RACING) func_80290338();
    }
    if (command == NET_LEAVE) {
        session.Close(); raceActive = false;
        loadConsumed = false; loadedEpoch = ~0u; remoteControllers = {};
        if (gGamestate == RACING) func_80290338();
    }
    command = 0;
    if (session.Active() && session.IsHost() && !session.LocalOnly() && session.settings.scope == 1)
        discovery.Advertise(uint16_t(port), session.SessionId(), contentHash(), session.settings,
            uint8_t(session.roster.size()), session.PasswordRequired(), session.phase);
    else discovery.StopAdvertising();
    discovery.Poll();
    if (!session.Active()) return;
    auto oldSnapshot = session.snapshot;
    auto oldCount = session.receivedSnapshots;
    auto oldPhase = session.phase;
    session.Poll();
    for (const auto& identity : session.roster) {
        if (identity.connection == session.LocalConnection() && identity.controller < party.size()) {
            party[identity.controller].character = identity.character;
            party[identity.controller].transmission = identity.transmission;
            party[identity.controller].ready = identity.ready;
        }
    }
    if (oldPhase != session.phase && session.phase == KartNet::Phase::Lobby && raceActive) {
        raceActive = loadConsumed = false; loadedEpoch = ~0u;
        if (gGamestate == RACING) func_80290338();
    }
    if (oldPhase != session.phase) {
        if (session.phase == KartNet::Phase::Loading) loadConsumed = false;
        if (session.phase == KartNet::Phase::Racing) CVarSetInteger("gNet.Window", 0);
        if (session.phase == KartNet::Phase::Disconnected || session.phase == KartNet::Phase::Error ||
            session.phase == KartNet::Phase::Finished) CVarSetInteger("gNet.Window", 1);
    }
    if (oldCount && oldCount != session.receivedSnapshots) previous = oldSnapshot;
    if (session.IsHost() && session.CanStart() && CVarGetInteger("gNet.AutoStart", 0) && startRulesAvailable(session.settings)) session.Start();
    if (!raceActive) return;
    // Keep camera/look controls local; only driving intent is sent to the authority.
    std::array<OSContPad, 4> physical;
    std::copy_n(gControllerPads, 4, physical.begin());
    for (const auto& identity : session.roster) {
        if (identity.connection != session.LocalConnection() || identity.controller >= party.size()) continue;
        const int slot = session.EngineSlot(identity.participant);
        const int source = party[identity.controller].controller;
        if (slot >= 0 && slot < 4 && source < 4)
            game_options_net_input_for(slot, physical[source].button, physical[source].stick_x, physical[source].stick_y);
    }
    std::fill_n(gControllerPads, 4, OSContPad{});
    for (const auto& identity : session.roster) {
        const int slot = session.EngineSlot(identity.participant);
        if (slot < 0 || slot >= 8) continue;
        if (identity.connection != session.LocalConnection()) {
            if (session.IsHost()) {
                const auto remote = session.InputFor(identity.participant);
                updateRemoteController(remote, remoteControllers[slot]);
                session.inputAck = std::max(session.inputAck, remote.sequence);
            }
            continue;
        }
        if (identity.controller >= party.size() || party[identity.controller].controller >= 4 || slot >= 4) continue;
        auto pad = physical[party[identity.controller].controller];
        KartNet::Input input; input.participant = identity.participant; input.sequence = ++sequence;
        input.buttons = pad.button & KartNet::DrivingButtonMask;
        input.x = int8_t(std::clamp(int(pad.stick_x), -80, 80)); input.y = int8_t(std::clamp(int(pad.stick_y), -80, 80));
        input.throttle = uint8_t(std::clamp(std::max(pad.button_value[15], pad.right_trigger), 0.0f, 1.0f)*255);
        input.brake = uint8_t(std::clamp(std::max(pad.button_value[14], pad.left_trigger), 0.0f, 1.0f)*255);
        const int menuOwner = game_options_local_owner();
        if ((menuOwner >= 0 && (session.LocalOnly() || menuOwner == slot)) || session.phase != KartNet::Phase::Racing) {
            input.buttons = 0; input.x = input.y = 0; input.throttle = input.brake = 0;
            pad = {};
        }
        session.Submit(input);
        // Start stays local; camera/look commands remain in the local pad only.
        pad.button &= ~uint32_t(START_BUTTON);
        gControllerPads[slot] = pad;
    }
    gIsGamePaused = 0;
}
extern "C" int net_take_load(void) {
    if (session.phase != KartNet::Phase::Loading || (loadConsumed && loadedEpoch == session.RaceEpoch())) return 0;
    resetRace(); loadedEpoch = session.RaceEpoch();
    loadConsumed = raceActive = true; return 1;
}
extern "C" void net_configure_race(void) {
    const auto& rules = session.settings;
    gModeSelection = rules.mode == 0 ? GRAND_PRIX : rules.mode == 1 ? VERSUS : BATTLE;
    gPlaceItemBoxes = rules.scope == 0 && rules.items != 2; gCCSelection = rules.cc;
    set_mirror_mode(rules.mirror);
    gPlayerCount = gPlayerCountSelection1 = std::clamp(int(session.LocalCount()), 1, 4);
    gScreenModeSelection = gPlayerCount == 1 ? SCREEN_MODE_1P :
        gPlayerCount == 2 ? SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL : SCREEN_MODE_3P_4P_SPLITSCREEN;
    gScreenModeListIndex = gPlayerCount == 1 ? 0 : gPlayerCount == 2 ? 1 : gPlayerCount;
    for (int i = 0; i < 8; ++i) kart_transmission_set_mode(i, KART_TRANSMISSION_AUTOMATIC);
    for (const auto& p : session.roster) {
        int slot = session.EngineSlot(p.participant);
        if (slot < 0 || slot >= 8) continue;
        if (slot < 4) gCharacterSelections[slot] = p.character;
        kart_transmission_set_mode(slot, KartTransmissionMode(p.transmission));
    }
}
extern "C" int net_spawn_players(float x, float z, float y) {
    if (!raceActive) return 0;
    unsigned usedCharacters = 0;
    for (const auto& p : session.roster) usedCharacters |= 1u << p.character;
    for (int slot = 0; slot < 8; ++slot) {
        const auto* identity = identityForSlot(slot);
        const int racer = globalRacerForSlot(slot);
        int selected = identity ? identity->character : 0;
        if (!identity && slot < racerCount()) {
            while (selected < 7 && (usedCharacters & (1u << selected))) ++selected;
            usedCharacters |= 1u << selected;
        }
        const auto type = slot >= racerCount() ? 0 : PLAYER_EXISTS | PLAYER_START_SEQUENCE | (identity ? PLAYER_HUMAN : PLAYER_CPU);
        float px = x + (racer % 2 ? 20.0f : -20.0f), pz = z+30.0f+(racer/2)*35.0f, py = y, yaw = 32768.0f;
        if (session.settings.mode == 2) {
            // Reuse the stock arena cardinal pads; the second row sits alongside each pad.
            const float radius = IsBigDonut() ? 575.0f : IsSkyscraper() ? 400.0f : IsDoubleDeck() ? 160.0f : 200.0f;
            py = IsBigDonut() ? 200.0f : IsSkyscraper() ? 480.0f : IsDoubleDeck() ? 55.0f : 5.0f;
            const float side = racer >= 4 ? 50.0f : 0.0f;
            static const float normalYaw[] = {32768.0f, 0.0f, -16384.0f, 16384.0f};
            static const float donutYaw[] = {-16384.0f, 16384.0f, 0.0f, 32768.0f};
            const int pad = racer % 4;
            px = pad < 2 ? side : pad == 2 ? -radius : radius;
            pz = pad >= 2 ? side : pad == 0 ? radius : -radius;
            yaw = IsBigDonut() ? donutYaw[pad] : normalYaw[pad];
        }
        spawn_player(&gPlayers[slot], slot, px, pz, py, yaw, selected, type);
        if (session.settings.mode == 2) {
            gPlayerBalloonCount[slot] = int(session.settings.balloons)-1;
            for (int balloon = session.settings.balloons; balloon < 3; ++balloon) gPlayerBalloonStatus[slot][balloon] = 0;
        }
        gPlayers[slot].currentRank = gGPCurrentRaceRankByPlayerId[slot] = racer;
        gGPCurrentRacePlayerIdByRank[racer] = gPlayerPositionLUT[racer] = slot;
    }
    D_80164A28 = 0; func_80039AE4(); return 1;
}
extern "C" int net_before_tick(void) {
    if (!raceActive) return 1;
    if (net_world_paused()) return 0;
    if (gRaceState == RACE_STAGING && session.phase == KartNet::Phase::Loading) session.Loaded();
    if (session.phase == KartNet::Phase::Loading) return gRaceState < RACE_STAGING;
    if (session.phase != KartNet::Phase::Racing && session.phase != KartNet::Phase::Finished) return 0;
    if (gRaceState == RACE_STAGING && net_running()) {
        start_race(); CVarSetInteger("gNet.Window", 0);
        D_8018D178 = 0; D_8018D2BC = 1; D_8018D2A4 = 1;
    }
    if (!session.IsHost()) {
        applyState();
        // Client renders replicated karts. Physics, checkpoints and finish decisions remain on host.
        // Host cameras tick before physics, while tire contacts retain that same pose.
        // Use the corresponding replicated pre-physics anchor, not the newer body center.
        std::array<std::array<float, 3>, 4> positions;
        for (int slot = 0; slot < session.LocalCount(); ++slot) {
            std::copy_n(gPlayers[slot].pos, 3, positions[slot].begin());
            std::copy_n(gPlayers[slot].oldPos, 3, gPlayers[slot].pos);
        }
        CM_TickCameras();
        for (int slot = 0; slot < session.LocalCount(); ++slot)
            std::copy_n(positions[slot].begin(), 3, gPlayers[slot].pos);
        func_80059AC8(); CM_TickActors();
        if (gTickCounter & 1) CM_TickActors60fps();
        if (OnlineItemWorldEnabled) KartNet::ApplyItemWorld(session);
        return 0;
    }
    for (int i = 0; i < racerCount(); ++i) gPlayers[i].lapCount = std::clamp(gLapCountByPlayerId[i], 0, int(session.settings.laps));
    return session.phase == KartNet::Phase::Racing;
}
extern "C" void net_update_display(void) {
    if (!raceActive) return;
    if (!session.IsHost()) return;
    if (session.settings.mode == 2) {
        std::array<int, 8> alive{}; int count = 0;
        for (int slot = 0; slot < racerCount(); ++slot) {
            const auto* identity = identityForSlot(slot);
            if (identity && !identity->connected) gPlayerBalloonCount[slot] = -1;
            if (gPlayerBalloonCount[slot] < 0) {
                if (finishOrder[slot] < 0) finishOrder[slot] = racerCount()-1-finishCount++;
                gPlayers[slot].type |= PLAYER_CINEMATIC_MODE;
                const int rank = finishOrder[slot];
                gPlayers[slot].currentRank = gGPCurrentRaceRankByPlayerId[slot] = rank;
                gGPCurrentRacePlayerIdByRank[rank] = gPlayerPositionLUT[rank] = slot;
            } else alive[count++] = slot;
        }
        std::stable_sort(alive.begin(), alive.begin()+count, [](int a, int b) { return gPlayerBalloonCount[a] > gPlayerBalloonCount[b]; });
        for (int rank = 0; rank < count; ++rank) {
            const int slot = alive[rank];
            gPlayers[slot].currentRank = gGPCurrentRaceRankByPlayerId[slot] = rank;
            gGPCurrentRacePlayerIdByRank[rank] = gPlayerPositionLUT[rank] = slot;
        }
        return;
    }
    std::array<int, 8> order;
    for (int i = 0; i < 8; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.begin()+racerCount(), [](int a, int b) {
        if (finishOrder[a] >= 0 || finishOrder[b] >= 0) {
            if ((finishOrder[a] >= 0) != (finishOrder[b] >= 0)) return finishOrder[a] >= 0;
            return finishOrder[a] < finishOrder[b];
        }
        if (gLapCountByPlayerId[a] != gLapCountByPlayerId[b]) return gLapCountByPlayerId[a] > gLapCountByPlayerId[b];
        return gCourseCompletionPercentByPlayerId[a] > gCourseCompletionPercentByPlayerId[b];
    });
    for (int rank = 0; rank < 8; ++rank) {
        const int slot = order[rank];
        if (rank < racerCount() && finishOrder[slot] < 0 && gLapCountByPlayerId[slot] >= session.settings.laps) {
            finishOrder[slot] = finishCount++;
            gPlayers[slot].type |= PLAYER_CPU;
            if (identityForSlot(slot) && firstFinishTime < 0.0f) firstFinishTime = gCourseTimer;
        }
        if (finishOrder[slot] >= 0) gLapCountByPlayerId[slot] = gPlayers[slot].lapCount = session.settings.laps;
        gPlayers[slot].currentRank = gGPCurrentRaceRankByPlayerId[slot] = rank;
        gGPCurrentRacePlayerIdByRank[rank] = gPlayerPositionLUT[rank] = slot;
    }
}
extern "C" void net_after_frame(void) {
    if (!raceActive || !session.IsHost() || session.phase != KartNet::Phase::Racing) return;
    net_update_display();
    bool finished = !session.roster.empty();
    for (const auto& p : session.roster) {
        const int slot = session.EngineSlot(p.participant);
        if (p.connected && slot >= 0 && finishOrder[slot] < 0) finished = false;
    }
    // Finishers retain their positions. Remaining drivers receive their progress order after one minute.
    if (firstFinishTime >= 0.0f && gCourseTimer-firstFinishTime >= 60.0f) finished = true;
    if (session.settings.mode == 2) {
        int alive = 0;
        for (int slot = 0; slot < racerCount(); ++slot) if (gPlayerBalloonCount[slot] >= 0) ++alive;
        finished = alive <= 1 && racerCount() >= 2;
    }
    auto m = captureState();
    if (!session.LocalOnly() && OnlineItemWorldEnabled) KartNet::CaptureItemWorld(session, finished);
    session.Publish(m, finished);
    if (finished) CVarSetInteger("gNet.Window", 1);
}

namespace KartNet {
nlohmann::json DebugState() {
    nlohmann::json result = {{"phase", PhaseName(session.phase)}, {"host", session.IsHost()},
        {"participant", session.LocalParticipant()}, {"received_snapshots", session.receivedSnapshots},
        {"input_ack", session.inputAck}, {"snapshot_age_ms", session.SnapshotAge()}, {"rtt_ms", session.Rtt()},
        {"rejected", session.rejected}, {"sent", session.sent}, {"received", session.received},
        {"error", session.error}, {"viewports", gPlayerCountSelection1}, {"racers", nlohmann::json::array()}};
    result["round"] = session.RaceIndex(); result["cup_complete"] = session.CupComplete(); result["standings"] = session.standings;
    if (raceActive) for (int slot = 0; slot < racerCount(); ++slot) {
        int id = session.ParticipantForSlot(slot); auto& p = gPlayers[slot];
        result["racers"].push_back({{"participant", id}, {"slot", slot}, {"character", p.characterId},
            {"position", {p.pos[0], p.pos[1], p.pos[2]}}, {"speed", p.speed}, {"lap", p.lapCount}, {"rank", p.currentRank},
            {"display_rank", D_8018CF98[slot]}});
    }
    return result;
}
void DrawWindow() {
    ImGui::TextUnformatted(PhaseName(session.phase));
    if (!session.error.empty()) ImGui::TextWrapped("%s", session.error.c_str());
    if (!session.Active() && gGamestate == RACING) {
        if (ImGui::Button("Return to Menu")) command = 4;
        return;
    }
    if (!session.Active() && gGamestate != RACING) {
        static const char* names[] = {"Mario", "Luigi", "Yoshi", "Toad", "DK", "Wario", "Peach", "Bowser"};
        if (ImGui::Combo("Character", &character, names, 8)) party[0].character = uint8_t(character);
        ImGui::InputText("Host IPv4", address, sizeof(address));
        ImGui::InputInt("UDP Port", &port); port = std::clamp(port, 1, 65535);
        if (ImGui::Button("Host LAN")) command = 1;
        ImGui::SameLine(); if (ImGui::Button("Join LAN")) command = 2;
    } else {
        ImGui::Text("%s | %u ms", session.IsHost() ? "Host" : "Client", session.Rtt());
        if (session.CanStart())
            if (ImGui::Button("Start Race")) command = 3;
        if (session.phase == Phase::Finished) {
            for (const auto& k : session.snapshot.karts) ImGui::Text("Racer %u: lap %u / %u", k.participant, k.lap, session.settings.laps);
        }
        if (ImGui::Button("Leave LAN")) command = 4;
    }
}
}
