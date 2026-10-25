#include "AndroidPhone.h"
#if defined(__ANDROID__) || defined(ANDROID_PHONE_SELF_TEST)
#include <atomic>
#include <cstdint>
#include <mutex>
#include "defines.h"

namespace {
constexpr int classifyTouchMode(int game, int nextGame, int race, unsigned playerType,
                                bool paused, bool demo, bool leaving, bool portGui) {
    if (portGui) return ANDROID_PHONE_TOUCH_PORT_GUI;
    // Countdown accepts launch input. Rank calculation may still have a human racing in splitscreen.
    const bool driving = race == RACE_STAGING || race == RACE_IN_PROGRESS || race == RACE_CALCULATE_RANKS;
    const bool human = (playerType & (PLAYER_EXISTS | PLAYER_HUMAN)) == (PLAYER_EXISTS | PLAYER_HUMAN)
                       && !(playerType & (PLAYER_CPU | PLAYER_CINEMATIC_MODE));
    return game == RACING && nextGame == RACING && driving && human && !paused && !demo && !leaving
               ? ANDROID_PHONE_TOUCH_GAMEPLAY : ANDROID_PHONE_TOUCH_HIDDEN;
}

uint64_t packTouch(int buttons, int x, int y, int lookX, int lookY) {
    return uint32_t(buttons) | (uint64_t(uint8_t(x)) << 32) | (uint64_t(uint8_t(y)) << 40)
           | (uint64_t(uint8_t(lookX)) << 48) | (uint64_t(uint8_t(lookY)) << 56);
}

struct TouchState {
    std::atomic<uint32_t> token{ANDROID_PHONE_TOUCH_HIDDEN};
    std::atomic<uint64_t> pad{0};
    std::mutex transition;

    void publishMode(int mode) {
        std::lock_guard<std::mutex> lock(transition);
        const uint32_t previous = token.load();
        if ((previous & ANDROID_PHONE_TOUCH_MODE_MASK) == uint32_t(mode)) return;
        pad.store(0);
        // An epoch also catches a close/reopen between two Java polls.
        token.store(((previous + 4u) & 0x7ffffffcu) | uint32_t(mode));
    }

    void write(uint32_t expectedToken, uint64_t input) {
        std::lock_guard<std::mutex> lock(transition);
        if (token.load() == expectedToken &&
            (expectedToken & ANDROID_PHONE_TOUCH_MODE_MASK) == ANDROID_PHONE_TOUCH_GAMEPLAY) pad.store(input);
    }
};
TouchState touch;
}
#endif

#ifdef __ANDROID__
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>
#include <libultraship.h>
#include <imgui_internal.h>
extern "C" {
#include "main.h"
#include "code_800029B0.h"
#include "menus.h"
#include "kart_input.h"
#include "kart_transmission.h"
}

namespace {
std::atomic<int> touchLayout{0}, touchTransmission{0};
std::atomic<bool> touchDispatchTest{false};

bool portGuiOpen() {
    auto* context = Ship::Context::GetRawInstance();
    if (!context || !context->GetWindow()) return true;
    const auto gui = context->GetWindow()->GetGui();
    if (!gui) return true;
    if (gui->GetMenuOrMenubarVisible()) return true;
    auto* imgui = ImGui::GetCurrentContext();
    if (!imgui) return false;
    if (!imgui->OpenPopupStack.empty()) return true;
    // Floating port windows can remain open after the menu bar is closed.
    for (const auto* window : imgui->Windows) {
        if (!window->Active || window->Hidden || window->IsFallbackWindow ||
            (window->Flags & (ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_ChildWindow |
                              ImGuiWindowFlags_DockNodeHost | ImGuiWindowFlags_Tooltip))) continue;
        if (window->ID != gui->GetMainGameWindowID() && std::strcmp(window->Name, "Main - Deck") != 0) {
            static ImGuiID last = 0;
            if (last != window->ID) {
                last = window->ID;
                __android_log_print(ANDROID_LOG_INFO, "KartTouch", "UI owns touch: %s", window->Name);
            }
            return true;
        }
    }
    return false;
}
}

extern "C" int android_phone_touch_mode(void) {
    return int(touch.token.load() & ANDROID_PHONE_TOUCH_MODE_MASK);
}

extern "C" int android_phone_touch_dispatch_test(void) { return touchDispatchTest.load() ? 1 : 0; }

extern "C" JNIEXPORT jint JNICALL
Java_dev_spaghettikart_phone_MainActivity_touchMode(JNIEnv*, jclass) { return jint(touch.token.load()); }

extern "C" JNIEXPORT jint JNICALL
Java_dev_spaghettikart_phone_MainActivity_touchTransmission(JNIEnv*, jclass) { return touchTransmission.load(); }

extern "C" JNIEXPORT jint JNICALL
Java_dev_spaghettikart_phone_MainActivity_touchLayout(JNIEnv*, jclass) { return touchLayout.load(); }

extern "C" int android_phone_touch_layout(void) {
    int layout = CVarGetInteger("gPhone.HudLayout", 0) == 1 ? 1 : 0;
    touchLayout.store(layout);
    return layout;
}

extern "C" JNIEXPORT jlong JNICALL
Java_dev_spaghettikart_phone_MainActivity_touchSnapshot(JNIEnv*, jclass) {
    return touch.pad.load();
}

extern "C" JNIEXPORT void JNICALL
Java_dev_spaghettikart_phone_MainActivity_touchPad(JNIEnv*, jclass, jint token, jint buttons, jint x, jint y, jint lookX, jint lookY) {
    touch.write(uint32_t(token), packTouch(buttons, x, y, lookX, lookY));
}

extern "C" void android_phone_merge_pad(void) {
    // Only the game thread reads engine globals and ImGui. JNI reads published atomics only.
    const int mode = classifyTouchMode(gGamestate, gGamestateNext, gRaceState, gPlayers[0].type,
                                       gIsGamePaused != 0, gDemoMode != 0 || gDemoUseController != 0,
                                       gIsInQuitToMenuTransition != 0, portGuiOpen());
    if (mode != android_phone_touch_mode())
        __android_log_print(ANDROID_LOG_INFO, "KartTouch", "mode=%d game=%d next=%d race=%d player=%x pause=%d demo=%d/%d leaving=%d",
            mode, gGamestate, gGamestateNext, gRaceState, gPlayers[0].type, gIsGamePaused,
            gDemoMode, gDemoUseController, gIsInQuitToMenuTransition);
    touch.publishMode(mode);
    android_phone_touch_layout();
    const bool manual = mode == ANDROID_PHONE_TOUCH_GAMEPLAY && !kart_transmission_is_automatic(&gPlayers[0]);
    touchTransmission.store(manual ? 0x100 | (kart_transmission_get_gear(0)+1) : 0);
    constexpr uint32_t shiftButtons = 0x01fe0000;
    static uint32_t previousShift = 0;
    if (mode != ANDROID_PHONE_TOUCH_GAMEPLAY) {
        previousShift = 0;
        return;
    }
    auto input = touch.pad.load();
    uint32_t shift = uint32_t(input) & shiftButtons;
    if (!manual) input &= ~uint64_t(shiftButtons | KART_CLUTCH_BUTTON);
    // A quick release can arrive between game frames. Preserve one clutch frame
    // when a new detent is consumed, then let the existing transmission engage it.
    else if (shift && shift != previousShift) input |= KART_CLUTCH_BUTTON;
    previousShift = manual ? shift : 0;
    auto& pad = gControllerPads[0];
    pad.button |= uint32_t(input);
    const auto x = int8_t(input >> 32), y = int8_t(input >> 40);
    if (std::abs(int(x)) > std::abs(int(pad.stick_x))) pad.stick_x = x;
    if (std::abs(int(y)) > std::abs(int(pad.stick_y))) pad.stick_y = y;
    const auto lookX = int8_t(input >> 48), lookY = int8_t(input >> 56);
    if (std::abs(int(lookX)) > std::abs(int(pad.right_stick_x))) pad.right_stick_x = lookX;
    if (std::abs(int(lookY)) > std::abs(int(pad.right_stick_y))) pad.right_stick_y = lookY;
    for (int bit = 0; bit < 32; ++bit)
        if (uint32_t(input) & (uint32_t(1) << bit)) pad.button_value[bit] = 1.0f;
}

extern "C" int android_phone_active(void) { return 1; }

extern "C" void android_phone_init(int argc, char** argv) {
    const char* path = SDL_AndroidGetExternalStoragePath();
    if (!path) return;
    chdir(path);
    bool testSession = false, dispatchTest = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--kart-touch-dispatch-test") == 0) dispatchTest = true;
        if (strncmp(argv[i], "--net-join=", 11) == 0) setenv("SK_NET_JOIN", argv[i]+11, 1);
        if (strcmp(argv[i], "--kart-test") == 0) {
            testSession = true;
            auto root = std::filesystem::path(path) / "kart-test";
            std::filesystem::create_directories(root);
            setenv("SK_TEST_ROOT", root.string().c_str(), 1);
        }
    }
    touchDispatchTest.store(testSession && dispatchTest);
    if (std::getenv("SK_TEST_ROOT")) {
        for (int i = 1; i < argc; ++i)
            if (strcmp(argv[i], "--kart-audio-test") == 0) setenv("SK_AUDIO_PROBE", "1", 1);
    }
    // First-run defaults only. Existing preferences and saves are left intact.
    if (!std::filesystem::exists("spaghettify.cfg.json")) {
        nlohmann::json config = {{"Window", {{"Backend", {{"Id", 2}, {"Name", "OpenGL"}}}, {"Fullscreen", {{"Enabled", true}}}}},
            {"CVars", {{"gFlatViewMode", 1}, {"gSkipIntro", 3}, {"gInterpolationFPS", 60}}}};
        std::ofstream("spaghettify.cfg.json") << config.dump(2);
    }
    if (const char* testRoot = std::getenv("SK_TEST_ROOT")) {
        // Async settings saves during shutdown must never persist test mute/quick-boot to normal play.
        std::filesystem::copy_file("spaghettify.cfg.json", std::filesystem::path(testRoot) / "spaghettify.cfg.json",
                                   std::filesystem::copy_options::overwrite_existing);
    }
}
#else
extern "C" void android_phone_merge_pad(void) {}
extern "C" int android_phone_active(void) { return 0; }
extern "C" int android_phone_touch_layout(void) { return 0; }
extern "C" int android_phone_touch_mode(void) { return ANDROID_PHONE_TOUCH_HIDDEN; }
extern "C" int android_phone_touch_dispatch_test(void) { return 0; }
extern "C" void android_phone_init(int, char**) {}
#endif

#ifdef ANDROID_PHONE_SELF_TEST
// Standalone: c++ -std=c++17 -pthread -DANDROID_PHONE_SELF_TEST -Iinclude src/port/AndroidPhone.cpp
#include <cassert>
#include <iostream>
#include <thread>

int main() {
    constexpr unsigned human = PLAYER_EXISTS | PLAYER_HUMAN;
    unsigned cases = 0;
    for (int game = 0; game < 10; ++game) {
        for (int race = RACE_INIT; race <= RACE_EXIT; ++race) {
            for (unsigned flags = 0; flags < 128; ++flags) {
                const bool paused = flags & 1, demo = flags & 2, leaving = flags & 4, gui = flags & 8;
                unsigned player = (flags & 16) ? 0 : human;
                if (flags & 32) player |= PLAYER_CPU;
                if (flags & 64) player |= PLAYER_CINEMATIC_MODE;
                const bool racing = game == RACING && (race == RACE_STAGING || race == RACE_IN_PROGRESS ||
                                                       race == RACE_CALCULATE_RANKS);
                int expected = gui ? ANDROID_PHONE_TOUCH_PORT_GUI : ANDROID_PHONE_TOUCH_HIDDEN;
                if (!gui && racing && flags == 0) expected = ANDROID_PHONE_TOUCH_GAMEPLAY;
                assert(classifyTouchMode(game, game, race, player, paused, demo, leaving, gui) == expected);
                ++cases;
            }
        }
    }
    assert(classifyTouchMode(RACING, MAIN_MENU, RACE_IN_PROGRESS, human, false, false, false, false) == 0);
    assert(classifyTouchMode(RACING, RACING, RACE_STAGING, human | PLAYER_START_SEQUENCE,
                             false, false, false, false) == ANDROID_PHONE_TOUCH_GAMEPLAY);

    TouchState state;
    const auto input = packTouch(0x8040 | 0x10000 | 0x80000, -80, 80, 80, -80);
    assert(uint32_t(input) == (0x8040 | 0x10000 | 0x80000));
    assert(int8_t(input >> 32) == -80 && int8_t(input >> 40) == 80);
    assert(int8_t(input >> 48) == 80 && int8_t(input >> 56) == -80);
    state.write(0, input);
    assert(state.pad.load() == 0);
    state.publishMode(ANDROID_PHONE_TOUCH_GAMEPLAY);
    const auto first = state.token.load();
    state.write(first, input);
    assert(state.pad.load() == input);
    state.publishMode(ANDROID_PHONE_TOUCH_GAMEPLAY);
    assert(state.token.load() == first && state.pad.load() == input);
    for (int hidden : {ANDROID_PHONE_TOUCH_HIDDEN, ANDROID_PHONE_TOUCH_PORT_GUI}) {
        state.publishMode(hidden);
        assert(state.pad.load() == 0);
        state.write(state.token.load(), input);
        assert(state.pad.load() == 0);
        state.publishMode(ANDROID_PHONE_TOUCH_GAMEPLAY);
        state.write(first, input);
        assert(state.pad.load() == 0);
        state.write(state.token.load(), input);
        assert(state.pad.load() == input);
        state.write(state.token.load(), 0);
        assert(state.pad.load() == 0);
    }
    const auto stale = state.token.load();
    state.publishMode(ANDROID_PHONE_TOUCH_HIDDEN);
    state.publishMode(ANDROID_PHONE_TOUCH_GAMEPLAY);
    std::thread writer([&] {
        for (int i = 0; i < 10000; ++i) state.write(stale, input);
    });
    for (int i = 0; i < 10000; ++i) {
        state.publishMode(ANDROID_PHONE_TOUCH_HIDDEN);
        state.publishMode(ANDROID_PHONE_TOUCH_GAMEPLAY);
        assert(state.pad.load() == 0);
    }
    writer.join();
    assert(state.pad.load() == 0);
    std::cout << "PASS: " << cases << " native mode cases; packed camera/steering/buttons; stale epochs; 10000 concurrent transitions\n";
}
#endif
