#include <libultraship.h>
#include "libultraship/controller/wheel/WheelDevice.h"
#include "SpaghettiGui.h"
#include <ship/window/gui/Gui.h>
#include <ship/window/Window.h>
#include "ship/controller/controldeck/ControlDeck.h"
#include "ship/controller/controldevice/controller/Controller.h"
#include "ship/controller/controldevice/controller/ControllerRumble.h"
#include "ship/controller/controldevice/controller/mapping/ControllerRumbleMapping.h"
#include "ship/config/ConsoleVariable.h"
#ifdef __SWITCH__
#include "ConfigVersion.h"
#else
#include "ship/config/Config.h"
#endif

#ifdef __APPLE__
#include <SDL_hints.h>
#include <SDL_video.h>

#include "fast/backends/gfx_metal.h"
#include <imgui_impl_metal.h>
#include <imgui_impl_sdl2.h>
#else
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_video.h>
#endif

#if defined(__ANDROID__) || defined(__IOS__)
#include "port/mobile/MobileImpl.h"
#endif

#ifdef ENABLE_OPENGL
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#endif

#if defined(ENABLE_DX11) || defined(ENABLE_DX12)
#include <fast/backends/gfx_direct3d11.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

// NOLINTNEXTLINE
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif

#include <algorithm>
#include <cmath>
#include <cstdio>

extern "C" {
#include "defines.h"
#include "camera.h"
#include "code_800029B0.h"
#include "main.h"
#include "mk64.h"
#include "kart_input.h"
#include "kart_character_stats.h"
}

namespace Ship {
#define TOGGLE_BTN ImGuiKey_F1
#define TOGGLE_PAD_BTN ImGuiKey_GamepadBack

    static const char* GetSurfaceLabel(s32 surfaceType) {
        switch (surfaceType) {
            case SURFACE_DEFAULT:
                return "Default";
            case AIRBORNE:
                return "Airborne";
            case ASPHALT:
                return "Asphalt";
            case DIRT:
                return "Dirt";
            case SAND:
                return "Sand";
            case STONE:
                return "Stone";
            case SNOW:
                return "Snow";
            case BRIDGE:
                return "Bridge";
            case SAND_OFFROAD:
                return "Sand Offroad";
            case GRASS:
                return "Grass";
            case ICE:
                return "Ice";
            case WET_SAND:
                return "Wet Sand";
            case SNOW_OFFROAD:
                return "Snow Offroad";
            case CLIFF:
                return "Cliff";
            case DIRT_OFFROAD:
                return "Dirt Offroad";
            case TRAIN_TRACK:
                return "Train Track";
            case CAVE:
                return "Cave";
            case ROPE_BRIDGE:
                return "Rope Bridge";
            case WOOD_BRIDGE:
                return "Wood Bridge";
            case WATER_SURFACE:
                return "Water";
            case BOOST_RAMP_WOOD:
                return "Wood Boost Ramp";
            case OUT_OF_BOUNDS:
                return "Out Of Bounds";
            case BOOST_RAMP_ASPHALT:
                return "Asphalt Boost Ramp";
            case RAMP:
                return "Ramp";
            default:
                return "Unknown";
        }
    }

    static f32 GetSurfaceRoughnessForDebug(s32 surfaceType) {
        switch (surfaceType) {
            case AIRBORNE:
                return 0.0f;
            case ICE:
                return 0.05f;
            case RAMP:
            case BOOST_RAMP_WOOD:
            case BOOST_RAMP_ASPHALT:
                return 0.1f;
            case ASPHALT:
                return 0.15f;
            case GRASS:
                return 0.3f;
            case STONE:
            case CAVE:
                return 0.4f;
            case ROPE_BRIDGE:
            case WOOD_BRIDGE:
                return 0.7f;
            case SAND:
            case WET_SAND:
            case SNOW:
            case CLIFF:
            case OUT_OF_BOUNDS:
                return 0.8f;
            case DIRT:
                return 0.9f;
            case SAND_OFFROAD:
            case SNOW_OFFROAD:
            case DIRT_OFFROAD:
            case TRAIN_TRACK:
                return 1.0f;
            default:
                return 0.5f;
        }
    }

    static bool IsWheelForceFeedbackGrounded(const Player* player) {
        return player->surfaceType != AIRBORNE && player->collision.surfaceDistance[2] < 50.0f &&
               (player->effects & 8) == 0;
    }

    static bool IsWheelForceFeedbackHitActive(const Player* player) {
        return (player->effects & (HIT_BY_ITEM_EFFECT | HIT_EFFECT)) != 0;
    }

    static bool IsWheelForceFeedbackLightningActive(const Player* player) {
        return (player->effects & LIGHTNING_EFFECT) != 0;
    }

    static float ComputeWheelSlopeSteeringForce(const Player* player, bool grounded, float speedKmh) {
        if (!grounded) {
            return 0.0f;
        }

        const float downhillX = -player->collision.orientationVector[0];
        const float downhillZ = -player->collision.orientationVector[2];
        const float slopeSteepness = sqrtf((downhillX * downhillX) + (downhillZ * downhillZ));
        if (slopeSteepness < 0.02f) {
            return 0.0f;
        }

        const float normalizedDownhillX = downhillX / slopeSteepness;
        const float normalizedDownhillZ = downhillZ / slopeSteepness;
        const float yawRadians = -(player->rotation[1] + player->unk_0C0) * (6.28318530718f / 65536.0f);
        const float forwardX = sinf(yawRadians);
        const float forwardZ = cosf(yawRadians);
        const float rightX = forwardZ;
        const float rightZ = -forwardX;
        const float downhillToRight = (normalizedDownhillX * rightX) + (normalizedDownhillZ * rightZ);
        const float perpendicularToSlope = fabs(downhillToRight);
        const float speedScale = 0.35f + (std::clamp(speedKmh / 80.0f, 0.0f, 1.0f) * 0.65f);

        return std::clamp(downhillToRight * perpendicularToSlope * slopeSteepness * speedScale * 0.85f, -0.75f, 0.75f);
    }

    static float NormalizeArcadeKartFxRange(float value, float minValue, float maxValue) {
        if (maxValue <= minValue) {
            return std::clamp(value, 0.0f, 1.0f);
        }

        return std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
    }

    static float ApplyArcadeKartFxCurve(float value, float power) {
        return powf(std::clamp(value, 0.0f, 1.0f), std::clamp(power, 0.01f, 8.0f));
    }

    static void SetControllerRumbleForPort(uint8_t portIndex, float strength) {
        auto controlDeck = Context::GetInstance()->GetControlDeck();
        if (controlDeck == nullptr) {
            return;
        }

        auto controller = controlDeck->GetControllerByPort(portIndex);
        if (controller == nullptr || controller->GetRumble() == nullptr) {
            return;
        }

        auto rumble = controller->GetRumble();
        auto mappings = rumble->GetAllRumbleMappings();
        if (mappings.empty()) {
            return;
        }

        strength = std::clamp(strength, 0.0f, 1.0f);
        if (strength <= 0.01f) {
            rumble->StopRumble();
            return;
        }

        const float lowMax = std::clamp(CVarGetFloat("gArcadeKart.ControllerRumble.LowMotorMax", 70.0f), 0.0f, 100.0f);
        const float highMax = std::clamp(CVarGetFloat("gArcadeKart.ControllerRumble.HighMotorMax", 55.0f), 0.0f, 100.0f);
        const uint8_t lowPercent = static_cast<uint8_t>(std::clamp(strength * lowMax, 0.0f, 100.0f));
        const uint8_t highPercent = static_cast<uint8_t>(std::clamp(strength * highMax, 0.0f, 100.0f));

        for (auto& [id, mapping] : mappings) {
            mapping->SetLowFrequencyIntensity(lowPercent);
            mapping->SetHighFrequencyIntensity(highPercent);
        }
        rumble->StartRumble();

        if (portIndex == 0) {
            CVarSetFloat("gArcadeKart.DebugControllerRumbleStrength", strength);
            CVarSetInteger("gArcadeKart.DebugControllerRumbleLow", lowPercent);
            CVarSetInteger("gArcadeKart.DebugControllerRumbleHigh", highPercent);
        }
    }

    static void UpdateControllerSurfaceRumble() {
        if (CVarGetInteger("gArcadeKart.ControllerRumble.Enabled", 1) == 0 || gGamestate != RACING) {
            for (uint8_t portIndex = 0; portIndex < 4; portIndex++) {
                SetControllerRumbleForPort(portIndex, 0.0f);
            }
            return;
        }

        const float speedMinRatio = std::clamp(CVarGetFloat("gArcadeKart.PostFx.SpeedMinRatio", 0.0f), 0.0f, 1.0f);
        const float speedMaxRatio = std::clamp(CVarGetFloat("gArcadeKart.PostFx.SpeedMaxRatio", 1.0f), 0.0f, 3.0f);
        const float shakeResponsePower =
            std::clamp(CVarGetFloat("gArcadeKart.PostFx.ShakeResponsePower", 3.0f), 0.01f, 8.0f);
        const float outputScale =
            std::clamp(CVarGetFloat("gArcadeKart.ControllerRumble.OutputScale", 0.65f), 0.0f, 1.0f);

        for (uint8_t portIndex = 0; portIndex < 4; portIndex++) {
            char cvarName[96];

            snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.SpeedRatio", portIndex + 1);
            const float speedRatio = CVarGetFloat(cvarName, 0.0f);
            snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.RoadRoughness", portIndex + 1);
            const float roadRoughness = CVarGetFloat(cvarName, 0.0f);
            snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.DriftAmount", portIndex + 1);
            const float driftAmount = CVarGetFloat(cvarName, 0.0f);
            snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.BoostShakeAmount", portIndex + 1);
            const float boostShakeAmount = CVarGetFloat(cvarName, 0.0f);
            snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.FxScale", portIndex + 1);
            const float fxScale = CVarGetFloat(cvarName, 0.0f);

            const float normalizedSpeed = NormalizeArcadeKartFxRange(speedRatio, speedMinRatio, speedMaxRatio);
            const float rumbleInput =
                std::clamp((normalizedSpeed * std::clamp(roadRoughness, 0.0f, 1.0f)) +
                               (driftAmount > 0.0f ? 0.1f : 0.0f) + (boostShakeAmount > 0.0f ? 0.3f : 0.0f),
                           0.0f, 1.0f);
            const float rumbleStrength = ApplyArcadeKartFxCurve(rumbleInput, shakeResponsePower) *
                                         std::clamp(fxScale, 0.0f, 1.0f) * outputScale;

            SetControllerRumbleForPort(portIndex, rumbleStrength);
            if (portIndex == 0) {
                CVarSetFloat("gArcadeKart.DebugControllerRumbleInput", rumbleInput);
            }
        }
    }

    static void UpdateWheelForceFeedback() {
        if (gPlayerOne == nullptr) {
            LUS::WheelDeviceManager::Instance().UpdatePlayerOneMenuForceFeedback();
            return;
        }

        Player* player = gPlayerOne;
        float speedKmh = (player->speed / 18.0f) * 216.0f;
        bool grounded = IsWheelForceFeedbackGrounded(player);
        float slopeSteeringForce = ComputeWheelSlopeSteeringForce(player, grounded, speedKmh);
        float steeringSpringMultiplier = kart_character_stats_get_steering_spring_multiplier(player->characterId);
        Context::GetInstance()->GetConsoleVariables()->SetFloat("gArcadeKart.ControllerSlopeSteeringForce",
                                                                slopeSteeringForce);
        LUS::WheelDeviceManager::Instance().UpdatePlayerOneForceFeedback(speedKmh, slopeSteeringForce, grounded,
                                                                         IsWheelForceFeedbackHitActive(player),
                                                                         IsWheelForceFeedbackLightningActive(player),
                                                                         player->surfaceType, gCurrentCourseId,
                                                                         steeringSpringMultiplier);
    }

    static void DrawKartDebugTelemetry() {
        if (CVarGetInteger("gArcadeKart.DebugTelemetry.Enabled", 0) == 0) {
            return;
        }

        if (gPlayerOne == nullptr || gControllerOne == nullptr) {
            return;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 pivot(0.5f, 1.0f);
        const ImVec2 pos(viewport->WorkPos.x + (viewport->WorkSize.x * 0.5f),
                         viewport->WorkPos.y + viewport->WorkSize.y - 12.0f);
        Player* player = gPlayerOne;
        const f32 speed = (player->speed / 18.0f) * 216.0f;
        const s32 surfaceType = player->surfaceType;
        const f32 surfaceRoughness = GetSurfaceRoughnessForDebug(surfaceType);
        const f32 fxRoughness = CVarGetFloat("gArcadeKart.PostFx.Player1.RoadRoughness", surfaceRoughness);
        const f32 rumbleInput = CVarGetFloat("gArcadeKart.DebugControllerRumbleInput", 0.0f);
        const f32 surfaceRumble = CVarGetFloat("gArcadeKart.DebugForceSurfaceRumble", 0.0f);
        const bool grounded = IsWheelForceFeedbackGrounded(player);
        const f32 slopeSteeringForce =
            ComputeWheelSlopeSteeringForce(player, grounded, speed);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("Kart Telemetry", nullptr, flags)) {
            ImGui::SetWindowFontScale(1.8f);
            ImGui::Text("Surface %s (%d)", GetSurfaceLabel(surfaceType), surfaceType);
            ImGui::Text("Tyres FL %d FR %d BL %d BR %d", player->tyres[FRONT_LEFT].surfaceType,
                        player->tyres[FRONT_RIGHT].surfaceType, player->tyres[BACK_LEFT].surfaceType,
                        player->tyres[BACK_RIGHT].surfaceType);
            ImGui::Text("Rough %.2f FX %.2f Rum %.2f Wheel %.2f", surfaceRoughness, fxRoughness, rumbleInput,
                        surfaceRumble);
            ImGui::Text("Ground %d Slope %.2f Speed %.1f", grounded ? 1 : 0, slopeSteeringForce, speed);
        }
        ImGui::End();
    }

    void SpaghettiGui::DrawMenu() {
        UpdateWheelForceFeedback();
        UpdateControllerSurfaceRumble();

        const std::shared_ptr<Window> wnd = Context::GetInstance()->GetWindow();
        const std::shared_ptr<Config> conf = Context::GetInstance()->GetConfig();

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground |
                                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                    ImGuiWindowFlags_NoResize;

        if (GetMenuBar() && GetMenuBar()->IsVisible()) {
            windowFlags |= ImGuiWindowFlags_MenuBar;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2((int)wnd->GetWidth(), (int)wnd->GetHeight()));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::Begin("Main - Deck", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        mTemporaryWindowPos = ImGui::GetWindowPos();

        const ImGuiID dockId = ImGui::GetID("main_dock");
        if (!ImGui::DockBuilderGetNode(dockId)) {
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_NoTabBar);
            ImGui::DockBuilderSetNodeSize(dockId, ImVec2(viewport->Size.x, viewport->Size.y));

            ImGui::DockBuilderDockWindow("Main Game", dockId);

            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImGuiID dockId = ImGui::GetID("main_dock");

            ImGuiID topId = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Up, 0.15f, nullptr, nullptr);
            ImGui::DockBuilderSetNodeSize(topId, ImVec2(viewport->Size.x, 40));

            ImGuiID bottomId = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Down, 0.25f, nullptr, nullptr);
            ImGui::DockBuilderSetNodeSize(bottomId, ImVec2(viewport->Size.x, viewport->Size.y * 0.1f));

            ImGuiID bottomLeftId = ImGui::DockBuilderSplitNode(bottomId, ImGuiDir_Left, 0.25f, nullptr, nullptr);
            ImGui::DockBuilderSetNodeSize(bottomId, ImVec2(viewport->Size.x, viewport->Size.y * 0.1f));

            ImGuiID rightId = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Right, 0.25f, nullptr, nullptr);
            ImGui::DockBuilderSetNodeSize(rightId, ImVec2(viewport->Size.x * 0.15f, viewport->Size.y));

            // Order of operations matters here for the properties window to be in the right spot
            ImGui::DockBuilderDockWindow("Scene Explorer", rightId);
            ImGui::DockBuilderDockWindow("Track Properties", rightId); // Attach as second tab

            ImGuiID rightBottomId = ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.25f, nullptr, nullptr);
            ImGui::DockBuilderSetNodeSize(rightBottomId, ImVec2(viewport->Size.x, viewport->Size.y * 0.25));

            ImGui::DockBuilderDockWindow("Properties", rightBottomId);
            ImGui::DockBuilderDockWindow("Tools", topId);
            ImGui::DockBuilderDockWindow("Content Browser", bottomLeftId);

            ImGui::DockBuilderFinish(dockId);
        }

        ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_NoDockingInCentralNode);

        if (ImGui::IsKeyPressed(TOGGLE_BTN) || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            (ImGui::IsKeyPressed(TOGGLE_PAD_BTN) && CVarGetInteger(CVAR_IMGUI_CONTROLLER_NAV, 0))) {
            if ((ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(TOGGLE_PAD_BTN)) && GetMenu()) {
                GetMenu()->ToggleVisibility();
            } else if ((ImGui::IsKeyPressed(TOGGLE_BTN) || ImGui::IsKeyPressed(TOGGLE_PAD_BTN)) && GetMenuBar()) {
                Gui::GetMenuBar()->ToggleVisibility();
            }
            if (wnd->IsFullscreen()) {
                Context::GetInstance()->GetWindow()->SetMouseCapture(
                    !(GetMenuOrMenubarVisible() || wnd->ShouldForceCursorVisibility()));
            }
            if (CVarGetInteger(CVAR_IMGUI_CONTROLLER_NAV, 0) && GetMenuOrMenubarVisible()) {
                mImGuiIo->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
            } else {
                mImGuiIo->ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
            }
        }

    #if __APPLE__
        if ((ImGui::IsKeyDown(ImGuiKey_LeftSuper) || ImGui::IsKeyDown(ImGuiKey_RightSuper)) &&
            ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            std::reinterpret_pointer_cast<ConsoleWindow>(
                Context::GetInstance()->GetWindow()->GetGui()->GetGuiWindow("Console"))
                ->Dispatch("reset");
        }
    #else
        if ((ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) &&
            ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            std::reinterpret_pointer_cast<ConsoleWindow>(
                Context::GetInstance()->GetWindow()->GetGui()->GetGuiWindow("Console"))
                ->Dispatch("reset");
        }
    #endif

        if (GetMenuBar()) {
            GetMenuBar()->Update();
            GetMenuBar()->Draw();
        }

        if (GetMenu()) {
            GetMenu()->Update();
            GetMenu()->Draw();
        }

        for (auto& windowIter : mGuiWindows) {
            windowIter.second->Update();
            windowIter.second->Draw();
        }

        DrawKartDebugTelemetry();

        ImGui::End();
    }

}
