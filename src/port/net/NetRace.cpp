#include "NetRace.h"
#include "game_options.h"
#include "Session.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
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
#include "racing/race_logic.h"
}

namespace {
KartNet::Session session;
bool initialized = false, raceActive = false, loadConsumed = false;
uint32_t sequence = 0, hostTick = 0, seenSnapshot = 0, snapshotAt = 0;
KartNet::Message previous;
std::array<int, 2> gears{};
std::array<float, 2> rpms{};
std::array<std::array<uint32_t, 3>, 2> lapTimes{};
int command = 0;
char address[64] = "192.168.1.1";
int port = 54777, character = 0;
uint64_t contentHash() {
    // Compare the base game archive, not platform-specific archive packaging.
    std::ifstream in("mk64.o2r", std::ios::binary);
    if (!in) return 0;
    uint64_t hash = 14695981039346656037ull;
    std::array<char, 65536> bytes;
    while (in) {
        in.read(bytes.data(), bytes.size());
        for (std::streamsize i = 0; i < in.gcount(); ++i) { hash ^= uint8_t(bytes[i]); hash *= 1099511628211ull; }
    }
    return hash;
}
void beginSession(bool host) {
    if (gGamestate == RACING) return;
    raceActive = loadConsumed = false; sequence = hostTick = seenSnapshot = 0; lapTimes = {}; previous = {};
    uint64_t hash = contentHash();
    if (!hash) { session.error = "Base game archive was not found"; session.phase = KartNet::Phase::Error; return; }
    if (host) session.Host(uint16_t(port), hash, uint8_t(character), CVarGetString("gNet.BindAddress", "0.0.0.0"));
    else session.Join(address, uint16_t(port), hash, uint8_t(character));
    CVarSetInteger("gNet.Window", 1);
}
void copyInput(const KartNet::Input& input, OSContPad& pad) {
    pad = {}; pad.button = input.buttons; pad.stick_x = input.x; pad.stick_y = input.y;
    for (int bit = 0; bit < CONTROLLER_BUTTON_VALUE_COUNT; ++bit)
        pad.button_value[bit] = (input.buttons & (1u << bit)) ? 1.0f : 0.0f;
    pad.button_value[15] = input.throttle / 255.0f; pad.button_value[14] = input.brake / 255.0f;
}
KartNet::Message captureState() {
    KartNet::Message m; m.tick = ++hostTick; m.time = gCourseTimer;
    for (int i = 0; i < 2; ++i) {
        const auto& p = gPlayers[i]; auto& k = m.karts[i];
        k.participant = i+1; k.type = p.type; k.effects = p.effects;
        std::copy_n(p.pos, 3, k.pos.begin()); std::copy_n(p.velocity, 3, k.velocity.begin());
        std::copy_n(p.oldPos, 3, k.oldPos.begin());
        std::copy_n(p.rotation, 3, k.rotation.begin());
        for (int a = 0; a < 3; ++a) for (int b = 0; b < 3; ++b) k.orientation[a*3+b] = p.orientationMatrix[a][b];
        for (int tyre = 0; tyre < 4; ++tyre) for (int a = 0; a < 3; ++a) k.tyres[tyre*3+a] = p.tyres[tyre].pos[a];
        k.ground = {p.unk_1F8, p.unk_1FC, p.unk_230, p.unk_23C};
        k.speed = p.speed; k.rpm = kart_transmission_get_engine_rpm(&p, i);
        k.gear = int8_t(kart_transmission_get_gear(i)); k.steering = p.unk_07C;
        k.driftYaw = p.unk_0C0; k.slope = p.slopeAccel;
        k.lap = uint8_t(std::clamp(gLapCountByPlayerId[i], 0, 3));
        k.rank = uint8_t(std::clamp(int(p.currentRank), 0, 1));
        for (int lap = 0; lap < k.lap; ++lap)
            if (!lapTimes[i][lap]) lapTimes[i][lap] = uint32_t(gCourseTimer * 100);
        k.lapTimes = lapTimes[i];
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
    for (int i = 0; i < 2; ++i) {
        const auto& k = m.karts[i]; const auto& old = previous.karts[i];
        int slot = KartNet::LocalSlot(k.participant, session.LocalParticipant());
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
        p.lapCount = k.lap; gLapCountByPlayerId[slot] = k.lap;
        p.currentRank = k.rank; gGPCurrentRaceRankByPlayerId[slot] = k.rank;
        gGPCurrentRacePlayerIdByRank[k.rank] = gPlayerPositionLUT[k.rank] = slot;
        gears[slot] = k.gear; rpms[slot] = k.rpm;
        for (int lap = 0; lap < 3; ++lap) {
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
extern "C" int net_gear(int player) { return gears[std::clamp(player, 0, 1)]; }
extern "C" float net_rpm(int player) { return rpms[std::clamp(player, 0, 1)]; }

extern "C" void net_menu_state(NetMenuState* state) {
    if (!state) return;
    *state = {};
    state->active = session.Active(); state->inRace = raceActive;
    state->canStart = session.phase == KartNet::Phase::Lobby && session.IsHost() && session.HasPeer();
    state->character = character; state->port = port;
    std::snprintf(state->address, sizeof(state->address), "%s", address);
    if (!session.error.empty()) std::snprintf(state->status, sizeof(state->status), "%s", session.error.c_str());
    else std::snprintf(state->status, sizeof(state->status), "%s  %u MS", KartNet::PhaseName(session.phase), session.Rtt());
}

extern "C" void net_menu_adjust(int field, int direction) {
    if (session.Active() || gGamestate == RACING) return;
    const int step = direction ? std::clamp(direction, -10, 10) : 1;
    if (field == 0) { character = (character + step + 8) % 8; CVarSetInteger("gNet.Character", character); }
    else if (field == 1) { port = std::clamp(port + step, 1, 65535); CVarSetInteger("gNet.Port", port); }
    else if (field >= 2 && field <= 5) {
        int octets[4]{};
        std::sscanf(address, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]);
        octets[field - 2] = (std::clamp(octets[field - 2], 0, 255) + step + 256) % 256;
        std::snprintf(address, sizeof(address), "%d.%d.%d.%d", octets[0], octets[1], octets[2], octets[3]);
        CVarSetString("gNet.Address", address);
    }
}

extern "C" void net_menu_command(int action) {
    if ((action == 1 || action == 2) && !session.Active() && gGamestate != RACING) command = action;
    else if (action == 3 && session.phase == KartNet::Phase::Lobby && session.IsHost() && session.HasPeer()) command = action;
    else if (action == 4) command = action;
}

extern "C" void net_read_inputs(void) {
    if (!initialized) {
        initialized = true;
        port = std::clamp(CVarGetInteger("gNet.Port", 54777), 1, 65535);
        character = std::clamp(CVarGetInteger("gNet.Character", 0), 0, 7);
        const char* endpoint = std::getenv("SK_NET_JOIN");
        std::snprintf(address, sizeof(address), "%s", endpoint ? endpoint : CVarGetString("gNet.Address", "192.168.1.1"));
        command = endpoint ? 2 : CVarGetInteger("gNet.Mode", 0);
        CVarSetInteger("gNet.Mode", 0);
    }
    if (command == 1 || command == 2) beginSession(command == 1);
    if (command == 3) session.Start();
    if (command == 4) {
        session.Close(); raceActive = false;
        if (gGamestate == RACING) func_80290338();
    }
    command = 0;
    if (!session.Active()) return;
    auto oldSnapshot = session.snapshot;
    auto oldCount = session.receivedSnapshots;
    auto oldPhase = session.phase;
    session.Poll();
    if (oldPhase != session.phase) {
        if (session.phase == KartNet::Phase::Racing) CVarSetInteger("gNet.Window", 0);
        if (session.phase == KartNet::Phase::Disconnected || session.phase == KartNet::Phase::Error ||
            session.phase == KartNet::Phase::Finished) CVarSetInteger("gNet.Window", 1);
    }
    if (oldCount && oldCount != session.receivedSnapshots) previous = oldSnapshot;
    if (session.IsHost() && session.HasPeer() && CVarGetInteger("gNet.AutoStart", 0)) session.Start();
    if (!raceActive) return;
    // Keep camera/look controls local; only driving intent is sent to the authority.
    auto& pad = gControllerPads[0];
    game_options_net_input(pad.button, pad.stick_x, pad.stick_y);
    KartNet::Input input; input.sequence = ++sequence; input.buttons = pad.button & 0xc0d0;
    input.x = int8_t(std::clamp(int(pad.stick_x), -80, 80)); input.y = int8_t(std::clamp(int(pad.stick_y), -80, 80));
    input.throttle = uint8_t(std::clamp(pad.button_value[15], 0.0f, 1.0f)*255);
    input.brake = uint8_t(std::clamp(pad.button_value[14], 0.0f, 1.0f)*255);
    if (game_options_is_live_network()) {
        input.buttons = 0; input.x = input.y = 0; input.throttle = input.brake = 0;
        pad = {};
    }
    session.Submit(input);
    pad.button &= ~(uint32_t(START_BUTTON) | Z_TRIG);
    for (int slot = 1; slot < 4; ++slot) gControllerPads[slot] = {};
    if (session.IsHost()) {
        auto remote = session.RemoteInput(); copyInput(remote, gControllerPads[1]);
        if (remote.sequence) session.inputAck = remote.sequence;
    }
    gIsGamePaused = 0;
}
extern "C" int net_take_load(void) {
    if (session.phase != KartNet::Phase::Loading || loadConsumed) return 0;
    loadConsumed = raceActive = true; return 1;
}
extern "C" void net_configure_race(void) {
    gModeSelection = GRAND_PRIX; gPlaceItemBoxes = 0; gCCSelection = CC_50;
    gPlayerCount = gPlayerCountSelection1 = 1; gScreenModeSelection = SCREEN_MODE_1P;
    for (auto p : session.roster) gCharacterSelections[KartNet::LocalSlot(p.participant, session.LocalParticipant())] = p.character;
    for (int i = 0; i < 2; ++i) kart_transmission_set_mode(i, KART_TRANSMISSION_AUTOMATIC);
}
extern "C" int net_spawn_players(float x, float z, float y) {
    if (!raceActive) return 0;
    for (int slot = 0; slot < 8; ++slot) {
        int id = slot < 2 ? (slot == 0 ? session.LocalParticipant() : 3-session.LocalParticipant()) : 0;
        float side = id == 1 ? -20.0f : 20.0f;
        spawn_player(&gPlayers[slot], slot, x+side, z+30, y, 32768.0f,
            id ? session.roster[id-1].character : 0, id ? PLAYER_EXISTS | PLAYER_START_SEQUENCE | PLAYER_HUMAN : 0);
    }
    D_80164A28 = 0; func_80039AE4(); return 1;
}
extern "C" int net_before_tick(void) {
    if (!raceActive) return 1;
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
        std::array<float, 3> position;
        std::copy_n(gPlayers[0].pos, 3, position.begin());
        std::copy_n(gPlayers[0].oldPos, 3, gPlayers[0].pos);
        CM_TickCameras();
        std::copy_n(position.begin(), 3, gPlayers[0].pos);
        func_80059AC8(); CM_TickActors();
        if (gTickCounter & 1) CM_TickActors60fps();
        return 0;
    }
    for (int i = 0; i < 2; ++i) gPlayers[i].lapCount = std::clamp(gLapCountByPlayerId[i], 0, 3);
    return session.phase == KartNet::Phase::Racing;
}
extern "C" void net_update_display(void) {
    if (!raceActive) return;
    if (session.IsHost()) {
        int leader = gCourseCompletionPercentByPlayerId[1] > gCourseCompletionPercentByPlayerId[0] ? 1 : 0;
        for (int i = 0; i < 2; ++i) {
            int rank = i == leader ? 0 : 1;
            gPlayers[i].currentRank = gGPCurrentRaceRankByPlayerId[i] = rank;
            gGPCurrentRacePlayerIdByRank[rank] = gPlayerPositionLUT[rank] = i;
        }
    }
    for (int i = 2; i < 8; ++i) {
        gGPCurrentRaceRankByPlayerId[i] = i;
        gGPCurrentRacePlayerIdByRank[i] = gPlayerPositionLUT[i] = i;
    }
}
extern "C" void net_after_frame(void) {
    if (!raceActive || !session.IsHost() || session.phase != KartNet::Phase::Racing) return;
    auto m = captureState();
    bool finished = m.karts[0].lap >= 3 || m.karts[1].lap >= 3;
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
    if (raceActive) for (int id = 1; id <= 2; ++id) {
        int slot = LocalSlot(id, session.LocalParticipant()); auto& p = gPlayers[slot];
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
        ImGui::Combo("Character", &character, names, 8);
        ImGui::InputText("Host IPv4", address, sizeof(address));
        ImGui::InputInt("UDP Port", &port); port = std::clamp(port, 1, 65535);
        if (ImGui::Button("Host LAN")) command = 1;
        ImGui::SameLine(); if (ImGui::Button("Join LAN")) command = 2;
    } else {
        ImGui::Text("%s | %u ms", session.IsHost() ? "Host" : "Client", session.Rtt());
        if (session.phase == Phase::Lobby && session.IsHost() && session.HasPeer())
            if (ImGui::Button("Start Race")) command = 3;
        if (session.phase == Phase::Finished) {
            for (const auto& k : session.snapshot.karts) ImGui::Text("Racer %u: lap %u / 3", k.participant, k.lap);
        }
        if (ImGui::Button("Leave LAN")) command = 4;
    }
}
}
