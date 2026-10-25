#include "KartTestBridge.h"
#include "AndroidPhone.h"
#include "net/NetRace.h"
#include "audio/AudioProbe.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <nlohmann/json.hpp>
#include <fast/interpreter.h>
#include <fast/Fast3dWindow.h>
#include <fast/backends/gfx_rendering_api.h>
#include <libultraship.h>
#include "Engine.h"
#include "game_options.h"
extern "C" {
#include "main.h"
#include "code_800029B0.h"
#include "menus.h"
#include "menu_pointer.h"
#include "camera.h"
#include "first_person_kart.h"
#include "defines.h"
#include "kart_transmission.h"
#include "hud_race_layout.h"
#include "port/vr/vr.h"
}

namespace {
using json = nlohmann::json;
namespace fs = std::filesystem;
fs::path root;
bool initialized = false;
bool menuBoot = false;
uint64_t tick = 0;
std::deque<json> queue;
json current;
int remaining = 0;
std::string id, status = "idle", error, capture;
json captures = json::array();
json events = json::array();
float pointerX = 0.0f, pointerY = 0.0f;
int pointerDown = 0;
bool pointerUiActive = false;
float pointerUiX = 0.0f, pointerUiY = 0.0f;
int pointerUiDown = 0;
void cancelPointer() {
    menu_pointer_cancel();
    pointerDown = 0;
    pointerUiDown = 0;
    pointerUiActive = false;
}
#ifdef __ANDROID__
json physicalPad;
json controllers() {
    json result = json::array();
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        char guid[33];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
        auto controller = SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
        json item = {{"name", SDL_JoystickNameForIndex(i)}, {"guid", guid},
                     {"recognized", !!SDL_IsGameController(i)}, {"opened", controller != nullptr}};
        if (controller) {
            char* mapping = SDL_GameControllerMapping(controller);
            item["mapping"] = mapping ? mapping : "";
            SDL_free(mapping);
            item["axes"] = json::array();
            for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis)
                item["axes"].push_back(SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis(axis)));
        }
        result.push_back(item);
    }
    return result;
}
#endif
bool validName(const std::string& s) {
    return std::regex_match(s, std::regex("[A-Za-z0-9_-]{1,64}"));
}
void writeJson(const fs::path& path, const json& data) {
    auto temp = path;
    temp += ".tmp";
    {
        std::ofstream stream(temp);
        stream << data.dump(2);
        if (!stream) throw std::runtime_error("Report write failed");
    }
    std::error_code ec;
    fs::remove(path, ec);
    // Windows readers may briefly hold the destination without delete sharing.
    // Publish again next tick, without aborting an otherwise valid input queue.
    fs::rename(temp, path, ec);
}
json vec(const float* p) { return json::array({p[0], p[1], p[2]}); }
json rect(HudRect r) { return json::array({r.x, r.y, r.w, r.h}); }
json hudState() {
    json result = json::array();
    if (gGamestate != RACING) return result;
    for (int i = 0; i < gPlayerCountSelection1; ++i) {
        HudRaceLayout r;
        get_race_hud_layout(i, &r);
        result.push_back({{"viewport", rect(r.viewport)}, {"column", rect(r.column)},
                          {"laps", rect(r.laps)}, {"rpm", rect(r.rpm)}, {"map", rect(r.map)},
                          {"map_slot", rect(r.mapSlot)}, {"map_scale", r.mapScale}});
    }
    return result;
}
json menuState() {
    json optionRows = json::array();
    for (int i = 0; i < game_options_row_count(); ++i) {
        GameOptionRowInfo row{};
        if (game_options_row_info(i, &row)) optionRows.push_back({{"label", row.label}, {"value", row.value},
            {"index", row.index}, {"id", row.index * 3}, {"enabled", !!row.enabled},
            {"adjustable", !!row.adjustable}, {"visible", !!row.visible}});
    }
    json options = {{"open", !!game_options_is_open()}, {"page", game_options_page()},
        {"context", game_options_context()}, {"live_network", !!game_options_is_live_network()}, {"rows", optionRows}};
    json lobbyRows = json::array(), party = json::array(), roster = json::array();
    for (int i = 0; i < game_lobby_row_count(); ++i) {
        GameOptionRowInfo row{};
        if (game_lobby_row_info(i, &row)) lobbyRows.push_back({{"label", row.label}, {"value", row.value},
            {"index", row.index}, {"id", row.index * 3}, {"enabled", !!row.enabled},
            {"adjustable", !!row.adjustable}, {"visible", !!row.visible}});
    }
    for (int i = 0; i < 4; ++i) {
        NetPartyInfo info{}; net_party_info(i, &info);
        party.push_back({{"controller", i}, {"joined", !!info.joined}, {"character", info.character},
                         {"transmission", info.transmission}, {"ready", !!info.ready}});
    }
    for (int i = 0; i < 8; ++i) {
        NetRosterInfo info{};
        if (net_roster_info(i, &info)) roster.push_back({{"participant", info.participant}, {"connection", info.connection},
            {"controller", info.controller}, {"character", info.character}, {"ready", !!info.ready},
            {"local", !!info.local}, {"connected", !!info.connected}, {"rank", info.rank},
            {"points", info.points}, {"finished", !!info.finished}});
    }
    NetMenuState session{}; net_menu_state(&session);
    json lobby = {{"open", !!game_lobby_is_open()}, {"page", game_lobby_page()}, {"context", game_lobby_context()},
                  {"rows", lobbyRows}, {"party", party}, {"roster", roster}, {"phase", session.phase},
                  {"host", !!session.host}, {"local_only", !!session.localOnly}, {"capacity", session.maxPlayers},
                  {"mode", session.mode}, {"cup", session.cup}, {"course", session.course}, {"laps", session.laps},
                  {"round", session.raceIndex}, {"cup_complete", !!session.cupComplete}, {"can_start", !!session.canStart}};
    GameDriverSelectionInfo selection{}; game_lobby_driver_selection(&selection);
    json selectionPlayers = json::array();
    for (int slot = 0; slot < selection.localCount; ++slot)
        selectionPlayers.push_back({{"slot", slot}, {"controller", selection.ports[slot]},
            {"character_locked", !!selection.characterLocked[slot]}, {"transmission_locked", !!selection.transmissionLocked[slot]},
            {"viewport", {selection.x[slot], selection.y[slot], selection.width[slot], selection.height[slot]}}});
    lobby["driver_selection"] = {{"open", !!selection.open}, {"step", selection.step}, {"players", selectionPlayers}};
    json targets = json::array();
    const int count = menu_pointer_count();
    for (int i = 0; i < count; ++i) {
        int context, targetId;
        float x, y, w, h;
        if (menu_pointer_get(i, &context, &targetId, &x, &y, &w, &h))
            targets.push_back({{"context", context}, {"id", targetId},
                               {"x", x}, {"y", y}, {"w", w}, {"h", h}});
    }
    json players = json::array();
    for (int i = 0; i < 4; ++i) {
        players.push_back({{"character", gCharacterSelections[i]}, {"grid", gCharacterGridSelections[i]},
                           {"locked", !!gCharacterGridCharacterLocked[i]},
                           {"confirmed", !!gCharacterGridIsSelected[i]},
                           {"transmission", kart_transmission_get_mode(i)}});
    }
    return {{"selection", gMenuSelection}, {"main_selection", gMainMenuSelection},
            {"sub_selection", gSubMenuSelection}, {"player_selection", gPlayerSelectMenuSelection},
            {"player_count", gPlayerCount}, {"mode", gModeSelection}, {"cc", gCCSelection},
            {"cup", gCupSelection}, {"course", gCourseIndexInCup}, {"course_id", gCurrentCourseId},
            {"paused", gIsGamePaused}, {"fading", !!is_screen_being_faded()},
            {"players", players}, {"targets", targets}, {"sound_mode", gSoundMode}, {"options", options}, {"lobby", lobby}};
}
json state() {
    const auto& p = gPlayers[0];
    FirstPersonKartBodyPose pose{};
    get_first_person_kart_body_pose(&pose, &p);
    return {{"tick", tick}, {"gamestate", gGamestate}, {"race_state", gRaceState},
            {"character", p.characterId}, {"position", vec(p.pos)}, {"velocity", vec(p.velocity)},
            {"speed", p.speed}, {"yaw", p.rotation[1]}, {"kart_forward", vec(pose.forward)},
            {"gear", kart_transmission_get_gear(0)}, {"clutch", kart_transmission_get_clutch_amount(0)},
            {"automatic", kart_transmission_is_automatic(&p)},
            {"kart_position", vec(pose.pos)}, {"camera", vec(cameras[0].pos)},
            {"look_at", vec(cameras[0].lookAt)}, {"first_person", !!CM_IsFirstPersonViewEnabled(0)},
            {"vr_active", vr_is_active()}, {"vr_mode", vr_get_view_mode()},
            {"button", gControllers[0].button}, {"stick_x", gControllers[0].rawStickX},
            {"remaining", remaining}, {"queued", queue.size()}, {"hud", hudState()},
            {"menu", menuState()}, {"touch_mode", android_phone_touch_mode()},
            {"pointer", json::array({pointerX, pointerY, pointerDown})},
            {"pointer_ui", pointerUiActive ? json::array({pointerUiX, pointerUiY, pointerUiDown}) : json(nullptr)},
            {"aspect_ratio", OTRGetAspectRatio()}, {"menu_boot", menuBoot},
            {"network", KartNet::DebugState()}};
}
void stop(const char* reason) {
    cancelPointer();
    queue.clear(); current = json(); remaining = 0; capture.clear(); status = reason;
}
void accept() {
    auto file = root / "command.json";
    if (!fs::exists(file)) return;
    json cmd;
    try {
        { std::ifstream stream(file); stream >> cmd; }
        fs::remove(file);
        if (cmd.value("op", "run") == "stop") { stop("stopped"); return; }
        if (status == "running") throw std::runtime_error("A queue is already running");
        std::string nextId = cmd.at("id").get<std::string>();
        if (!validName(nextId)) throw std::runtime_error("Invalid run id");
        id = nextId;
        auto steps = cmd.at("steps");
        if (!steps.is_array() || steps.empty() || steps.size() > 128) throw std::runtime_error("Expected 1-128 steps");
        int total = 0;
        for (const auto& step : steps) {
            int frames = step.at("frames").get<int>();
            if (frames < 1 || frames > 1800) throw std::runtime_error("Frames must be 1-1800");
            total += frames;
            for (const char* axis : {"stick_x", "stick_y", "look_x", "look_y"}) {
                int v = step.value(axis, 0);
                if (v < -80 || v > 80) throw std::runtime_error("Axis outside -80..80");
            }
            if (step.contains("capture") && !validName(step.at("capture").get<std::string>()))
                throw std::runtime_error("Invalid capture name");
            if (step.contains("pointer") && step.contains("pointerUi"))
                throw std::runtime_error("Pointer and pointerUi are mutually exclusive");
            for (const char* field : {"pointer", "pointerUi"}) {
                if (!step.contains(field)) continue;
                const auto& pointer = step.at(field);
                if (!pointer.is_array() || pointer.size() != 3 ||
                    !pointer[0].is_number() || !pointer[1].is_number() || !pointer[2].is_number_integer())
                    throw std::runtime_error("Pointer must be [x, y, down] with numeric coordinates and integer down");
                const double x = pointer[0].get<double>(), y = pointer[1].get<double>();
                if (!std::isfinite(x) || !std::isfinite(y) ||
                    std::abs(x) > std::numeric_limits<float>::max() ||
                    std::abs(y) > std::numeric_limits<float>::max())
                    throw std::runtime_error("Pointer coordinates must be finite floats");
                if (pointer[2] != 0 && pointer[2] != 1)
                    throw std::runtime_error("Pointer down must be 0 or 1");
            }
            int character = step.value("character", 0);
            if (character < 0 || character > 7) throw std::runtime_error("Character outside 0..7");
            int view = step.value("view", 0);
            int phoneLayout = step.value("phone_layout", 0);
            int transmission = step.value("transmission", 0);
            if (transmission < 0 || transmission > 1) throw std::runtime_error("Transmission must be 0 or 1");
            if (phoneLayout < 0 || phoneLayout > 1) throw std::runtime_error("Phone layout must be 0 or 1");
            if (view < 0 || view > 1) throw std::runtime_error("View must be 0 or 1");
            int vrView = step.value("vr_view", 0);
            if (vrView < 0 || vrView > 3) throw std::runtime_error("VR view must be 0..3");
            if (step.contains("stereo") && !step["stereo"].is_boolean()) throw std::runtime_error("Stereo must be boolean");
            auto buttons = step.value("buttons", int64_t(0));
            if (buttons < 0 || buttons > UINT32_MAX) throw std::runtime_error("Invalid button mask");
            for (const char* axis : {"throttle", "brake"}) {
                float value = step.value(axis, 0.f);
                if (!std::isfinite(value) || value < 0.f || value > 1.f) throw std::runtime_error("Pedal outside 0..1");
            }
            if (step.contains("pads")) {
                const auto& pads = step.at("pads");
                if (!pads.is_array() || pads.empty() || pads.size() > 4) throw std::runtime_error("Expected 1-4 local pads");
                unsigned ports = 0;
                for (const auto& pad : pads) {
                    int port = pad.at("port").get<int>();
                    if (port < 0 || port > 3 || (ports & (1u << port))) throw std::runtime_error("Duplicate or invalid local pad port");
                    ports |= 1u << port;
                    auto mask = pad.value("buttons", int64_t(0));
                    if (mask < 0 || mask > UINT32_MAX) throw std::runtime_error("Invalid local pad button mask");
                    for (const char* axis : {"stick_x", "stick_y", "look_x", "look_y"}) {
                        int value = pad.value(axis, 0);
                        if (value < -80 || value > 80) throw std::runtime_error("Local pad axis outside -80..80");
                    }
                    for (const char* axis : {"throttle", "brake"}) {
                        float value = pad.value(axis, 0.f);
                        if (!std::isfinite(value) || value < 0.f || value > 1.f) throw std::runtime_error("Local pad pedal outside 0..1");
                    }
                }
            }
        }
        if (total > 9000) throw std::runtime_error("Queue exceeds 9000 ticks");
        id = nextId; error.clear(); captures = json::array(); events = json::array();
        cancelPointer();
        queue = std::deque<json>(steps.begin(), steps.end());
        status = "running";
    } catch (const std::exception& ex) {
        error = ex.what();
        if (status != "running") stop("error");
    }
}
}

extern "C" int kart_test_active(void) {
    if (!initialized) {
        initialized = true;
        const char* path = std::getenv("SK_TEST_ROOT");
        if (path && *path) {
            root = fs::absolute(path);
            fs::create_directories(root);
            menuBoot = fs::is_regular_file(root / "menu-boot");
        }
    }
    return !root.empty();
}

extern "C" int kart_test_menu_boot(void) {
    return kart_test_active() && menuBoot;
}

extern "C" int kart_test_pointer_ui(float* x, float* y, int* down) {
    if (!kart_test_active() || !pointerUiActive || !x || !y || !down) return 0;
    *x = pointerUiX;
    *y = pointerUiY;
    *down = pointerUiDown;
    return 1;
}

extern "C" void kart_test_read_inputs(OSContPad* pads) {
    if (!kart_test_active()) return;
    ++tick;
#ifdef __ANDROID__
    // Observe the real SDL -> ControlDeck path before the isolated test queue replaces it.
    const auto& pad = pads[0];
    physicalPad = {{"button", pad.button}, {"stick_x", pad.stick_x}, {"stick_y", pad.stick_y},
                   {"look_x", pad.right_stick_x}, {"look_y", pad.right_stick_y},
                   {"throttle", pad.button_value[15]}, {"brake", pad.button_value[14]}};
#endif
    // Test instances never merge physical controllers or write normal play settings.
    if (!android_phone_touch_dispatch_test()) std::fill_n(pads, 4, OSContPad{});
    for (const char* name : {"gGameMasterVolume", "gMainMusicVolume", "gSFXMusicVolume", "gEnvironmentVolume"})
        CVarSetFloat(name, KartAudio::ProbeEnabled() ? 1.0f : 0.0f);
    try {
        accept();
        if (status != "running") {
            if (!android_phone_touch_dispatch_test()) cancelPointer();
            return;
        }
        auto beat = root / "heartbeat";
        if (!fs::exists(beat) || fs::file_time_type::clock::now() - fs::last_write_time(beat) > std::chrono::seconds(3)) {
            stop("heartbeat_expired"); return;
        }
        if (remaining == 0) {
            if (queue.empty()) { stop("complete"); return; }
            current = queue.front(); queue.pop_front();
            remaining = current.at("frames").get<int>();
            if (current.contains("character") && gGamestate == RACING)
                gPlayers[0].characterId = current.at("character").get<int>();
            if (current.contains("view")) CVarSetInteger("gFlatViewMode", current.at("view").get<int>());
            if (current.contains("phone_layout")) CVarSetInteger("gPhone.HudLayout", current.at("phone_layout").get<int>());
            if (current.contains("transmission") && !net_race_active())
                kart_transmission_set_mode(0, KartTransmissionMode(current.at("transmission").get<int>()));
            if (current.contains("vr_view")) CVarSetInteger("gVRViewMode", current.at("vr_view").get<int>());
            events.push_back({{"start", state()}, {"step", current}});
        }
        const auto applyPad = [](const json& input, OSContPad& pad) {
            pad = {};
            pad.button = input.value("buttons", uint32_t(0));
            pad.stick_x = input.value("stick_x", 0); pad.stick_y = input.value("stick_y", 0);
            pad.right_stick_x = input.value("look_x", 0); pad.right_stick_y = input.value("look_y", 0);
            for (int i = 0; i < CONTROLLER_BUTTON_VALUE_COUNT; ++i)
                pad.button_value[i] = (pad.button & (CONTROLLERBUTTONS_T(1) << i)) ? 1.0f : 0.0f;
            pad.button_value[15] = input.value("throttle", pad.button_value[15]);
            pad.button_value[14] = input.value("brake", pad.button_value[14]);
            pad.right_trigger = pad.button_value[15]; pad.left_trigger = pad.button_value[14];
        };
        applyPad(current, pads[0]);
        if (current.contains("pads")) for (const auto& input : current.at("pads")) applyPad(input, pads[input.at("port").get<int>()]);
        if (current.contains("pointer")) {
            if (pointerUiActive) cancelPointer();
            const auto& pointer = current.at("pointer");
            pointerX = pointer[0].get<float>();
            pointerY = pointer[1].get<float>();
            pointerDown = pointer[2].get<int>();
            menu_pointer_inject(pointerX, pointerY, pointerDown);
        } else if (current.contains("pointerUi")) {
            if (!pointerUiActive) cancelPointer();
            const auto& pointer = current.at("pointerUi");
            pointerUiX = pointer[0].get<float>();
            pointerUiY = pointer[1].get<float>();
            pointerUiDown = pointer[2].get<int>();
            pointerUiActive = true;
        } else {
            // An omitted pointer cancels a hold; only an explicit release can click.
            cancelPointer();
        }
        --remaining;
        if (!remaining) capture = current.value("capture", "");
    } catch (const std::exception& ex) { error = ex.what(); stop("error"); }
}

namespace KartTest {
bool WantsStereo() { return kart_test_active() && !capture.empty() && current.value("stereo", false); }
void Capture(Fast::Interpreter* renderer, int framebuffer, int width, int height, const char* eye) {
    if (!kart_test_active() || capture.empty()) return;
    try {
        if (width < 1 || height < 1 || width > 8192 || height > 8192) throw std::runtime_error("Invalid capture dimensions");
        std::vector<uint16_t> pixels(size_t(width) * height);
        renderer->mRapi->ReadFramebufferToCPU(framebuffer, width, height, pixels.data());
        std::string name = id + "-" + capture + "-" + eye + ".ppm";
        std::ofstream stream(root / name, std::ios::binary);
        stream << "P6\n" << width << " " << height << "\n255\n";
        // Managed eye targets already invert Y; only the GL desktop backbuffer needs flipping.
        bool flip = framebuffer == 0 && Ship::Context::GetRawInstance()->GetWindow()->GetWindowBackend() == Fast::WindowBackend::FAST3D_SDL_OPENGL;
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            uint16_t p = pixels[size_t(flip ? height - 1 - y : y) * width + x];
            char rgb[3] = {char(((p >> 11) & 31) * 255 / 31), char(((p >> 6) & 31) * 255 / 31), char(((p >> 1) & 31) * 255 / 31)};
            stream.write(rgb, 3);
        }
        if (!stream) throw std::runtime_error("Capture write failed");
        captures.push_back({{"file", name}, {"eye", eye}, {"state", state()}});
        if (std::string(eye) != "left") capture.clear();
    } catch (const std::exception& ex) { error = ex.what(); stop("error"); }
}
void Publish() {
    if (!kart_test_active()) return;
    try {
        if (status == "running" && !remaining && !events.empty() && !events.back().contains("end"))
            events.back()["end"] = state();
        json report = {{"id", id}, {"status", status}, {"error", error},
                       {"state", state()}, {"events", events}, {"captures", captures}};
#ifdef __ANDROID__
        report["physical_pad"] = physicalPad;
        report["sdl_controllers"] = controllers();
#endif
        writeJson(root / "state.json", report);
    } catch (const std::exception& ex) { error = ex.what(); stop("error"); }
}
}
