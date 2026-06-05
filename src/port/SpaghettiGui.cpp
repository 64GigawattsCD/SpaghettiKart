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
#include "camera.h"
#include "code_800029B0.h"
#include "main.h"
#include "mk64.h"
#include "kart_input.h"
#include "kart_transmission.h"
#include "kart_character_stats.h"
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
        if (CVarGetInteger("gArcadeKart.DebugTelemetry.Enabled", 1) == 0) {
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
        const s32 playerIndex = 0;
        const f32 speed = (player->speed / 18.0f) * 216.0f;
        const f32 rpm = kart_transmission_get_engine_rpm(player, playerIndex) *
                        CVarGetFloat("gArcadeKart.RpmDisplayMultiplier", 4.5f);
        const f32 slopeSteeringForce =
            ComputeWheelSlopeSteeringForce(player, IsWheelForceFeedbackGrounded(player), speed);
        const f32 springRawBase = CVarGetFloat("gArcadeKart.DebugSpringRawBase", 0.0f);
        const f32 springBase = CVarGetFloat("gArcadeKart.DebugSpringBase", 0.0f);
        const f32 springCharacterMultiplier = CVarGetFloat("gArcadeKart.DebugSpringCharacterMultiplier", 1.0f);
        const f32 springSurfaceMultiplier = CVarGetFloat("gArcadeKart.DebugSpringSurfaceMultiplier", 1.0f);
        const f32 springPercent = CVarGetFloat("gArcadeKart.DebugSpringPercent", 0.0f);
        const f32 springBaselineMultiplier = CVarGetFloat("gArcadeKart.DebugSpringBaselineMultiplier", 1.0f);
        const f32 springCenteringRatio = CVarGetFloat("gArcadeKart.DebugSpringCenteringRatio", 0.0f);
        const s32 springGrounded = CVarGetInteger("gArcadeKart.DebugSpringGrounded", 0);
        const s32 springSurface = CVarGetInteger("gArcadeKart.DebugSpringSurface", 0);
        const s32 springTunePressed = CVarGetInteger("gArcadeKart.DebugSpringTunePressed", 0);
        const s32 profilerRequested = CVarGetInteger("gArcadeKart.DebugSpringProfilerRequested", 0);
        const s32 profilerLast = CVarGetInteger("gArcadeKart.DebugSpringProfilerLast", -1);
        const s32 profilerSkipped = CVarGetInteger("gArcadeKart.DebugSpringProfilerSkipped", 0);
        const s32 profilerEnabled = CVarGetInteger("gArcadeKart.DebugSpringProfilerEnabled", 0);
        const s32 profilerDriverWrite = CVarGetInteger("gArcadeKart.DebugSpringProfilerDriverWrite", 0);
        const s32 profilerGlobalWrite = CVarGetInteger("gArcadeKart.DebugSpringProfilerGlobalWrite", 0);
        const s32 profilerWriteOk = CVarGetInteger("gArcadeKart.DebugSpringProfilerWriteOk", 0);
        const s32 profilerDriverValue = CVarGetInteger("gArcadeKart.DebugSpringProfilerDriverValue", 0);
        const s32 sdlCenteringEnabled = CVarGetInteger("gArcadeKart.DebugSpringSdlCenteringEnabled", 0);
        const s32 sdlAutocenterWriteOk = CVarGetInteger("gArcadeKart.DebugSpringSdlAutocenterWriteOk", 0);
        const s32 sdlAutocenterPercent = CVarGetInteger("gArcadeKart.DebugSpringSdlAutocenterPercent", 0);
        const s32 hapticOpen = CVarGetInteger("gArcadeKart.DebugSpringHapticOpen", 0);
        const s32 hapticSpring = CVarGetInteger("gArcadeKart.DebugSpringSupportsSpring", 0);
        const s32 hapticConstant = CVarGetInteger("gArcadeKart.DebugSpringSupportsConstant", 0);
        const s32 hapticPeriodic = CVarGetInteger("gArcadeKart.DebugSpringSupportsPeriodic", 0);
        const s32 hapticRumble = CVarGetInteger("gArcadeKart.DebugSpringSupportsRumble", 0);
        const s32 hapticActive = CVarGetInteger("gArcadeKart.DebugSpringHapticEffectActive", 0);
        const s32 hapticWriteOk = CVarGetInteger("gArcadeKart.DebugSpringHapticWriteOk", 0);
        const s32 hapticEffectId = CVarGetInteger("gArcadeKart.DebugSpringHapticEffectId", -1);
        const s32 hapticCoefficient = CVarGetInteger("gArcadeKart.DebugSpringHapticCoefficient", 0);
        const s32 hapticSaturation = CVarGetInteger("gArcadeKart.DebugSpringHapticSaturation", 0);
        const s32 hapticDeadband = CVarGetInteger("gArcadeKart.DebugSpringHapticDeadband", 0);
        const s32 logitechSdkLoaded = CVarGetInteger("gArcadeKart.DebugLogitechSdkLoaded", 0);
        const s32 logitechSdkInitialized = CVarGetInteger("gArcadeKart.DebugLogitechSdkInitialized", 0);
        const s32 logitechSdkWorkerRunning = CVarGetInteger("gArcadeKart.DebugLogitechSdkWorkerRunning", 0);
        const s32 logitechSdkCurrentOk = CVarGetInteger("gArcadeKart.DebugLogitechSdkCurrentOk", 0);
        const s32 logitechSdkSetPreferredOk = CVarGetInteger("gArcadeKart.DebugLogitechSdkSetPreferredOk", 0);
        const s32 logitechSdkPlaySpringOk = CVarGetInteger("gArcadeKart.DebugLogitechSdkPlaySpringOk", 0);
        const s32 logitechSdkSpringGain = CVarGetInteger("gArcadeKart.DebugLogitechSdkSpringGain", 0);
        const s32 logitechSdkDefaultSpringGain = CVarGetInteger("gArcadeKart.DebugLogitechSdkDefaultSpringGain", 0);
        const s32 logitechSdkSpringRequested = CVarGetInteger("gArcadeKart.DebugLogitechSdkSpringRequested", 0);
        const s32 logitechSdkSpringApplied = CVarGetInteger("gArcadeKart.DebugLogitechSdkSpringApplied", 0);
        const s32 logitechSdkSpringObserved = CVarGetInteger("gArcadeKart.DebugLogitechSdkSpringObserved", 0);
        const f32 logitechSdkSpringPercent = CVarGetFloat("gArcadeKart.LogitechSdkSpringPercent", 0.0f);
        const f32 forceConstant = CVarGetFloat("gArcadeKart.DebugForceConstantSigned", 0.0f);
        const f32 terrainKick = CVarGetFloat("gArcadeKart.DebugForceTerrainKick", 0.0f);
        const f32 coarseKick = CVarGetFloat("gArcadeKart.DebugForceCoarseKick", 0.0f);
        const f32 surfaceRumble = CVarGetFloat("gArcadeKart.DebugForceSurfaceRumble", 0.0f);
        const s32 shifterMask = CVarGetInteger("gArcadeKart.DebugShifterButtonMask", -1);
        const s32 shifterRequest = CVarGetInteger("gArcadeKart.DebugShifterRequestedGear", -2);
        const s32 shifterRaw = CVarGetInteger("gArcadeKart.DebugShifterRawGear", -2);
        const s32 shifterSmooth = CVarGetInteger("gArcadeKart.DebugShifterSmoothedGear", -2);
        const s32 shifterSmoothingFrames = CVarGetInteger("gArcadeKart.DebugShifterSmoothingFrames", 4);
        const s32 shifterNeutralSamples = CVarGetInteger("gArcadeKart.DebugShifterNeutralSamples", 0);
        const s32 shifterPressedCount = CVarGetInteger("gArcadeKart.DebugShifterPressedGearCount", 0);
        const s32 postFxEnabled = CVarGetInteger("gArcadeKart.PostFx.Enabled", 1);
        const s32 postFxManualOverride = CVarGetInteger("gArcadeKart.PostFx.ManualOverride", 0);
        const s32 postFxTuningSlidersOnly = CVarGetInteger("gArcadeKart.PostFx.TuningSlidersOnly", 0);
        const s32 postFxLayerHud = CVarGetInteger("gArcadeKart.PostFx.LayerHud", 1);
        const s32 postFxLayerActive = CVarGetInteger("gArcadeKart.PostFx.LayeredHudActive", 0);
        const s32 postFxSceneFb = CVarGetInteger("gArcadeKart.PostFx.SceneFramebufferId", -1);
        const s32 postFxHudFb = CVarGetInteger("gArcadeKart.PostFx.HudFramebufferId", -1);
        const f32 postFxShakeStrength = CVarGetFloat("gArcadeKart.PostFx.ShakeStrength", 0.018f);
        const f32 postFxShakeOutputScale = CVarGetFloat("gArcadeKart.PostFx.ShakeOutputScale", 0.3f);
        const f32 postFxWarpIntensity = CVarGetFloat("gArcadeKart.PostFx.WarpIntensity", 1.0f);
        const f32 postFxTestShake = CVarGetFloat("gArcadeKart.PostFx.TestShakeSlider", 0.0f);
        const f32 postFxTestWarp = CVarGetFloat("gArcadeKart.PostFx.TestWarpSlider", 0.0f);
        const f32 postFxSpeedMin = CVarGetFloat("gArcadeKart.PostFx.SpeedMinRatio", 0.0f);
        const f32 postFxSpeedMax = CVarGetFloat("gArcadeKart.PostFx.SpeedMaxRatio", 1.0f);
        const f32 postFxResponsePower = CVarGetFloat("gArcadeKart.PostFx.ResponsePower", 2.0f);
        const f32 postFxShakeResponsePower = CVarGetFloat("gArcadeKart.PostFx.ShakeResponsePower", 3.0f);
        const f32 postFxTestShakeMax = CVarGetFloat("gArcadeKart.PostFx.TuningShakeInputMax", 0.25f);
        const f32 postFxTestWarpMax = CVarGetFloat("gArcadeKart.PostFx.TuningWarpInputMax", 2.0f);
        const f32 postFxTuningShakeStrength = CVarGetFloat("gArcadeKart.PostFx.TuningShakeStrength", 0.04f);
        const f32 postFxTuningWarpStrength = CVarGetFloat("gArcadeKart.PostFx.TuningWarpStrength", 0.16f);
        const f32 postFxPlayerSpeed = CVarGetFloat("gArcadeKart.PostFx.Player1.SpeedRatio", 0.0f);
        const f32 postFxPlayerRoughness = CVarGetFloat("gArcadeKart.PostFx.Player1.RoadRoughness", 0.0f);
        const f32 postFxPlayerScale = CVarGetFloat("gArcadeKart.PostFx.Player1.FxScale", 1.0f);
        const f32 speedWideFov = CVarGetFloat("gArcadeKart.Camera.SpeedWideFov", 100.0f);
        const f32 speedNarrowFov = CVarGetFloat("gArcadeKart.Camera.SpeedNarrowFov", 75.0f);
        const f32 cameraSpeedRatio = CVarGetFloat("gArcadeKart.Camera.DebugSpeedRatio", 0.0f);
        const f32 cameraSpeedCurve = CVarGetFloat("gArcadeKart.Camera.DebugSpeedCurve", 0.0f);
        const f32 cameraTargetFov = CVarGetFloat("gArcadeKart.Camera.DebugTargetFov", speedWideFov);
        const f32 postFxActiveBarrel = CVarGetFloat("gArcadeKart.PostFx.DebugActiveBarrel", 0.0f);
        const f32 postFxActiveIntensity = CVarGetFloat("gArcadeKart.PostFx.DebugActiveIntensity", 0.0f);
        const f32 postFxActiveShakePixels = CVarGetFloat("gArcadeKart.PostFx.DebugActiveShakePixels", 0.0f);
        const f32 postFxSpeedCurve = CVarGetFloat("gArcadeKart.PostFx.DebugSpeedCurve", 0.0f);
        const f32 postFxRoughnessCurve = CVarGetFloat("gArcadeKart.PostFx.DebugRoughnessCurve", 0.0f);
        const f32 playerFov = (camera1 != nullptr) ? camera1->fieldOfView : 0.0f;
        const f32 rpmNeedleActual = CVarGetFloat("gArcadeKart.Hud.DebugRpmNeedleActualRpm", 0.0f);
        const f32 rpmNeedleScaled = CVarGetFloat("gArcadeKart.Hud.DebugRpmNeedleScaledRpm", 0.0f);
        const f32 rpmNeedleMax = CVarGetFloat("gArcadeKart.Hud.DebugRpmNeedleMotionMaxRpm", 7200.0f);
        const f32 rpmNeedleNormalized = CVarGetFloat("gArcadeKart.Hud.DebugRpmNeedleNormalized", 0.0f);
        const f32 rpmNeedleInputScale = CVarGetFloat("gArcadeKart.Hud.DebugRpmNeedleInputScale", 1.0f);
        const f32 rpmNeedleSweepDegrees = CVarGetFloat("gArcadeKart.Hud.DebugRpmNeedleSweepDegrees", 100.0f);

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
            ImGui::Text("Slope %.2f Ground %d Surf %d", slopeSteeringForce, springGrounded, springSurface);
            ImGui::Text("Spring %.0f%% Raw %.0f Clamp %.0f x%.1f", springPercent, springRawBase, springBase,
                        springBaselineMultiplier);
            ImGui::Text("Mult C%.2f S%.2f Ratio %.2f Tune %04X", springCharacterMultiplier,
                        springSurfaceMultiplier, springCenteringRatio, springTunePressed);
            ImGui::Text("Profiler En%d Req%d Last%d Skip%d", profilerEnabled, profilerRequested, profilerLast,
                        profilerSkipped);
            ImGui::Text("Reg OK%d D%d G%d Val%d", profilerWriteOk, profilerDriverWrite, profilerGlobalWrite,
                        profilerDriverValue);
            ImGui::Text("SDL Center %d Auto OK%d Pct%d", sdlCenteringEnabled, sdlAutocenterWriteOk,
                        sdlAutocenterPercent);
            ImGui::Text("Haptic O%d Sp%d C%d Per%d R%d", hapticOpen, hapticSpring, hapticConstant, hapticPeriodic,
                        hapticRumble);
            ImGui::Text("Haptic A%d W%d Id%d", hapticActive, hapticWriteOk, hapticEffectId);
            ImGui::Text("Coeff %d Sat %d Dead %d", hapticCoefficient, hapticSaturation, hapticDeadband);
            ImGui::Text("Logi SDK L%d I%d W%d Cur%d Pref%d Play%d", logitechSdkLoaded, logitechSdkInitialized,
                        logitechSdkWorkerRunning, logitechSdkCurrentOk, logitechSdkSetPreferredOk,
                        logitechSdkPlaySpringOk);
            ImGui::Text("Logi Spring %.0f%% Req%d App%d Obs%d SG%d DSG%d", logitechSdkSpringPercent,
                        logitechSdkSpringRequested, logitechSdkSpringApplied, logitechSdkSpringObserved,
                        logitechSdkSpringGain, logitechSdkDefaultSpringGain);
            ImGui::Text("Forces Const %.2f TK %.2f CK %.2f R %.2f", forceConstant, terrainKick, coarseKick,
                        surfaceRumble);
            ImGui::Text("PostFX En%d Man%d Sl%d", postFxEnabled, postFxManualOverride, postFxTuningSlidersOnly);
            ImGui::Text("PostFX Test Shake %.2f Warp %.2f", postFxTestShake, postFxTestWarp);
            ImGui::Text("PostFX RawMax Shake %.2f Warp %.2f", postFxTestShakeMax, postFxTestWarpMax);
            ImGui::Text("PostFX Speed %.2f Rough %.2f Scale %.2f", postFxPlayerSpeed, postFxPlayerRoughness,
                        postFxPlayerScale);
            ImGui::Text("Camera FOV %.1f Target %.1f %.2f/%.2f W%.0f N%.0f", playerFov, cameraTargetFov,
                        cameraSpeedRatio, cameraSpeedCurve, speedWideFov, speedNarrowFov);
            ImGui::Text("RPM Needle %.2f Raw %.0f Sc %.0f Max %.0f x%.2f Sw%.0f", rpmNeedleNormalized,
                        rpmNeedleActual, rpmNeedleScaled, rpmNeedleMax, rpmNeedleInputScale,
                        rpmNeedleSweepDegrees);
            ImGui::Text("PostFX Range Min %.2f Max %.2f P%.2f ShP%.2f", postFxSpeedMin, postFxSpeedMax,
                        postFxResponsePower, postFxShakeResponsePower);
            ImGui::Text("PostFX Gain Shake %.3f Warp %.2f", postFxTuningShakeStrength, postFxTuningWarpStrength);
            ImGui::Text("PostFX Raw Shake %.3f x%.2f Warp %.2f", postFxShakeStrength, postFxShakeOutputScale,
                        postFxWarpIntensity);
            ImGui::Text("PostFX Active I%.2f B%.3f Sh%.1f SC%.2f RC%.2f", postFxActiveIntensity, postFxActiveBarrel,
                        postFxActiveShakePixels, postFxSpeedCurve, postFxRoughnessCurve);
            ImGui::Text("Layer HUD %d Active %d Scene %d HUD %d", postFxLayerHud, postFxLayerActive, postFxSceneFb,
                        postFxHudFb);
            ImGui::Text("Shifter %02X Raw %s Sm %s Req %s", shifterMask, GetGearLabel(shifterRaw),
                        GetGearLabel(shifterSmooth), GetGearLabel(shifterRequest));
            ImGui::Text("Shifter Count %d Smooth %d N%d", shifterPressedCount, shifterSmoothingFrames,
                        shifterNeutralSamples);
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
