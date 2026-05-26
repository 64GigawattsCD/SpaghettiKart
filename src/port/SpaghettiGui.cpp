#include <libultraship.h>
#include "libultraship/controller/wheel/WheelDevice.h"
#include "SpaghettiGui.h"
#include <ship/window/gui/Gui.h>
#include <ship/window/Window.h>
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
#include "code_800029B0.h"
#include "main.h"
#include "mk64.h"
#include "kart_input.h"
#include "kart_transmission.h"
}

namespace Ship {
#define TOGGLE_BTN ImGuiKey_F1
#define TOGGLE_PAD_BTN ImGuiKey_GamepadBack

    static const char* GetGearLabel(s32 gear) {
        switch (gear) {
            case KART_GEAR_REVERSE:
                return "R";
            case KART_GEAR_NEUTRAL:
                return "N";
            default:
                break;
        }

        static char gearLabel[4];
        snprintf(gearLabel, sizeof(gearLabel), "%d", gear);
        return gearLabel;
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

    static void UpdateWheelForceFeedback() {
        if (gPlayerOne == nullptr) {
            return;
        }

        Player* player = gPlayerOne;
        float speedKmh = (player->speed / 18.0f) * 216.0f;
        bool grounded = IsWheelForceFeedbackGrounded(player);
        float slopeSteeringForce = ComputeWheelSlopeSteeringForce(player, grounded, speedKmh);
        Context::GetInstance()->GetConsoleVariables()->SetFloat("gArcadeKart.ControllerSlopeSteeringForce",
                                                                slopeSteeringForce);
        LUS::WheelDeviceManager::Instance().UpdatePlayerOneForceFeedback(speedKmh, slopeSteeringForce, grounded,
                                                                         IsWheelForceFeedbackHitActive(player),
                                                                         IsWheelForceFeedbackLightningActive(player),
                                                                         player->surfaceType, gCurrentCourseId);
    }

    static void DrawKartDebugTelemetry() {
        if (gPlayerOne == nullptr || gControllerOne == nullptr) {
            return;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 pivot(1.0f, 1.0f);
        const ImVec2 pos(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f,
                         viewport->WorkPos.y + viewport->WorkSize.y - 12.0f);
        Player* player = gPlayerOne;
        const s32 playerIndex = 0;
        const f32 speed = (player->speed / 18.0f) * 216.0f;
        const f32 rpm = kart_transmission_get_engine_rpm(player, playerIndex);
        const f32 throttle = kart_input_get_command_value(gControllerOne, KART_INPUT_THROTTLE);
        const f32 brake = kart_input_get_command_value(gControllerOne, KART_INPUT_BRAKE);
        const f32 clutch = kart_input_get_command_value(gControllerOne, KART_INPUT_CLUTCH);
        const f32 handbrake = kart_input_get_command_value(gControllerOne, KART_INPUT_DRIFT);
        const f32 forwardBack = kart_input_get_forward_backward_axis(gControllerOne);
        const f32 slopeSteeringForce =
            ComputeWheelSlopeSteeringForce(player, IsWheelForceFeedbackGrounded(player), speed);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("Kart Telemetry", nullptr, flags)) {
            ImGui::SetWindowFontScale(1.8f);
            ImGui::Text("Gear %s", GetGearLabel(kart_transmission_get_gear(playerIndex)));
            ImGui::Text("RPM  %.0f", rpm);
            ImGui::Text("Speed %.1f", speed);
            ImGui::Text("T %.2f  B %.2f  C %.2f", throttle, brake, clutch);
            ImGui::Text("H %.2f", handbrake);
            ImGui::Text("F/B %.2f", forwardBack);
            ImGui::Text("Slope %.2f", slopeSteeringForce);
        }
        ImGui::End();
    }

    void SpaghettiGui::DrawMenu() {
        UpdateWheelForceFeedback();

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
