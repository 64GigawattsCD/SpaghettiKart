#include "PortMenu.h"
#include "UIWidgets.h"
#include "port/Game.h"
#include "ship/window/gui/GuiMenuBar.h"
#include "ship/window/gui/GuiElement.h"
#include <variant>
#include "ship/utils/StringHelper.h"
#include <spdlog/fmt/fmt.h>
#include <variant>
#include <tuple>
#include "ResolutionEditor.h"

#include "engine/tracks/Track.h"
#include "engine/tracks/KalimariDesert.h"
#include "engine/tracks/ToadsTurnpike.h"

#ifdef __SWITCH__
#include <ship/port/switch/SwitchImpl.h>
#endif

extern "C" {
extern s32 gGamestateNext;
extern s32 gMenuSelection;
#include "audio/external.h"
#include "defines.h"
}

namespace GameUI {
extern std::shared_ptr<PortMenu> mPortMenu;

using namespace UIWidgets;

void PortMenu::AddSidebarEntry(std::string sectionName, std::string sidebarName, uint32_t columnCount) {
    assert(!sectionName.empty());
    assert(!sidebarName.empty());
    menuEntries.at(sectionName).sidebars.emplace(sidebarName, SidebarEntry{ .columnCount = columnCount });
    menuEntries.at(sectionName).sidebarOrder.push_back(sidebarName);
}

WidgetInfo& PortMenu::AddWidget(WidgetPath& pathInfo, std::string widgetName, WidgetType widgetType) {
    assert(!widgetName.empty());                        // Must be unique
    assert(menuEntries.contains(pathInfo.sectionName)); // Section/header must already exist
    assert(menuEntries.at(pathInfo.sectionName).sidebars.contains(pathInfo.sidebarName)); // Sidebar must already exist
    std::unordered_map<std::string, SidebarEntry>& sidebar = menuEntries.at(pathInfo.sectionName).sidebars;
    uint8_t column = pathInfo.column;
    if (sidebar.contains(pathInfo.sidebarName)) {
        while (sidebar.at(pathInfo.sidebarName).columnWidgets.size() < column + 1) {
            sidebar.at(pathInfo.sidebarName).columnWidgets.push_back({});
        }
    }
    SidebarEntry& entry = sidebar.at(pathInfo.sidebarName);
    entry.columnWidgets.at(column).push_back({ .name = widgetName, .type = widgetType });
    WidgetInfo& widget = entry.columnWidgets.at(column).back();
    switch (widgetType) {
        case WIDGET_CHECKBOX:
        case WIDGET_CVAR_CHECKBOX:
            widget.options = std::make_shared<CheckboxOptions>();
            break;
        case WIDGET_SLIDER_FLOAT:
        case WIDGET_CVAR_SLIDER_FLOAT:
            widget.options = std::make_shared<FloatSliderOptions>();
            break;
        case WIDGET_SLIDER_INT:
        case WIDGET_CVAR_SLIDER_INT:
            widget.options = std::make_shared<IntSliderOptions>();
            break;
        case WIDGET_COMBOBOX:
        case WIDGET_CVAR_COMBOBOX:
        case WIDGET_AUDIO_BACKEND:
        case WIDGET_VIDEO_BACKEND:
            widget.options = std::make_shared<ComboboxOptions>();
            break;
        case WIDGET_BUTTON:
            widget.options = std::make_shared<ButtonOptions>();
            break;
        case WIDGET_WINDOW_BUTTON:
            widget.options = std::make_shared<ButtonOptions>(ButtonOptions{ .size = Sizes::Inline });
            break;
        case WIDGET_COLOR_24:
        case WIDGET_COLOR_32:
            break;
        case WIDGET_SEARCH:
        case WIDGET_SEPARATOR:
        case WIDGET_SEPARATOR_TEXT:
        case WIDGET_TEXT:
        default:
            widget.options = std::make_shared<WidgetOptions>();
    }
    return widget;
}

void PortMenu::AddSettings() {
    // Add Settings menu
    AddMenuEntry("Settings", "gSettings.Menu.SettingsSidebarSection");
    // General Settings
    AddSidebarEntry("Settings", "General", 3);
    WidgetPath path = { "Settings", "General", SECTION_COLUMN_1 };

#ifdef __ANDROID__
    static const std::unordered_map<int32_t, const char*> phoneLayouts = {
        {0, "A: Controls inward"}, {1, "B: HUD inward"}
    };
    AddWidget(path, "Phone HUD Layout", WIDGET_CVAR_COMBOBOX)
        .CVar("gPhone.HudLayout").Options(ComboboxOptions().ComboMap(phoneLayouts));
#endif

    static bool isFullscreen = false;
#ifndef __SWITCH__
    AddWidget(path, "Toggle Fullscreen", WIDGET_CHECKBOX)
        .ValuePointer(&isFullscreen)
        .PreFunc([](WidgetInfo& info) { isFullscreen = Ship::Context::GetRawInstance()->GetWindow()->IsFullscreen(); })
        .Callback([](WidgetInfo& info) { Ship::Context::GetRawInstance()->GetWindow()->ToggleFullscreen(); })
        .Options(CheckboxOptions().Tooltip("Toggles Fullscreen On/Off."));
#endif

    AddWidget(path, "Startup Behaviour", WIDGET_CVAR_COMBOBOX)
        .CVar("gSkipIntro")
        .Options(ComboboxOptions()
            .ComboMap(introBehaviourOptions)
            .Tooltip("Select which scene or menu the game launch to."));

    AddWidget(path, "Menu Theme", WIDGET_CVAR_COMBOBOX)
        .CVar("gSettings.Menu.Theme")
        .Options(ComboboxOptions()
                     .Tooltip("Changes the Theme of the Menu Widgets.")
                     .ComboMap(menuThemeOptions)
                     .DefaultIndex(Colors::LightBlue));
    AddWidget(path, "Menu Extent", WIDGET_CVAR_COMBOBOX)
        .CVar("gSettings.Menu.Extent")
        .Options(ComboboxOptions()
                     .Tooltip("Changes the extent of the onscreen menu")
                     .ComboMap(menuExtentOptions)
                     .DefaultIndex(MenuExtent::Condensed));

    AddWidget(path, "Menu Scale: %.0fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gSettings.Menu.Scale")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gSettings.Menu.Extent", 0); })
        .Options(FloatSliderOptions()
                     .Tooltip("Adjust the scale for the menu.")
                     .Min(1.0f)
                     .Max(2.0f)
                     .DefaultValue(1.0f)
                     .Format("%.1f")
                     .Step(0.1f));
    AddWidget(path, "Controller pak screen", WIDGET_CVAR_CHECKBOX)
        .CVar("gControllerPakScreen")
        .Options(CheckboxOptions().Tooltip(
            "Enables the Controller Pak screen when starting the game."));
#if not defined(__SWITCH__) and not defined(__WIIU__)
    AddWidget(path, "Menu Controller Navigation", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_IMGUI_CONTROLLER_NAV)
        .Options(CheckboxOptions().Tooltip(
            "Allows controller navigation of the Spaghetti menu (Settings, Enhancements,...)\nCAUTION: "
            "This will disable game inputs while the menu is visible.\n\nD-pad to move between "
            "items, A to select, B to move up in scope."));
    AddWidget(path, "Cursor Always Visible", WIDGET_CVAR_CHECKBOX)
        .CVar("gSettings.CursorVisibility")
        .Callback([](WidgetInfo& info) {
            Ship::Context::GetRawInstance()->GetWindow()->SetForceCursorVisibility(
                CVarGetInteger("gSettings.CursorVisibility", 0));
        })
        .Options(CheckboxOptions().Tooltip("Makes the cursor always visible, even in full screen."));
#endif
    AddWidget(path, "Search In Sidebar", WIDGET_CVAR_CHECKBOX)
        .CVar("gSettings.Menu.SidebarSearch")
        .Callback([](WidgetInfo& info) {
            if (CVarGetInteger("gSettings.Menu.SidebarSearch", 0)) {
                mPortMenu->InsertSidebarSearch();
            } else {
                mPortMenu->RemoveSidebarSearch();
            }
        })
        .Options(CheckboxOptions().Tooltip(
            "Displays the Search menu as a sidebar entry in Settings instead of in the header."));
    AddWidget(path, "Search Input Autofocus", WIDGET_CVAR_CHECKBOX)
        .CVar("gSettings.Menu.SearchAutofocus")
        .Options(CheckboxOptions().Tooltip(
            "Search input box gets autofocus when visible. Does not affect using other widgets."));
    AddWidget(path, "Alt Assets Tab hotkey", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.Mods.AlternateAssetsHotkey")
        .Options(
            CheckboxOptions().Tooltip("Allows pressing the Tab key to toggle alternate assets").DefaultValue(true));
#ifndef __SWITCH__
    AddWidget(path, "Open App Files Folder", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            std::string filesPath = Ship::Context::GetRawInstance()->GetAppDirectoryPath();
            SDL_OpenURL(std::string("file:///" + std::filesystem::absolute(filesPath).string()).c_str());
        })
        .Options(ButtonOptions().Tooltip("Opens the folder that contains the save and mods folders, etc."));
#endif

    // Audio Settings
    path.sidebarName = "Audio";
    AddSidebarEntry("Settings", "Audio", 3);
    AddWidget(path, "Master Volume: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gGameMasterVolume")
        .Options(FloatSliderOptions()
                     .Tooltip("Adjust the overall sound volume.")
                     .ShowButtons(false)
                     .Format("")
                     .IsPercentage());
    AddWidget(path, "Main Music Volume: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gMainMusicVolume")
        .Options(FloatSliderOptions()
                     .Tooltip("Adjust the background music volume.")
                     .ShowButtons(false)
                     .Format("")
                     .IsPercentage());
    AddWidget(path, "Sound Effects Volume: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gSFXMusicVolume")
        .Options(FloatSliderOptions()
                     .Tooltip("Adjust the sound effects volume.")
                     .ShowButtons(false)
                     .Format("")
                     .IsPercentage());
    AddWidget(path, "Environment Volume: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gEnvironmentVolume")
        .Options(FloatSliderOptions()
                     .Tooltip("Adjust the environment volume.")
                     .ShowButtons(false)
                     .Format("")
                     .IsPercentage());
    AddWidget(path, "Audio API", WIDGET_AUDIO_BACKEND);

    // Graphics Settings
    static int32_t maxFps;
    const char* tooltip = "";
    if (Ship::Context::GetRawInstance()->GetWindow()->GetWindowBackend() == Fast::WindowBackend::FAST3D_DXGI_DX11) {
        maxFps = MAX_FPS;
        tooltip = "Uses Matrix Interpolation to create extra frames, resulting in smoother graphics. This is "
                  "purely visual and does not impact game logic, execution of glitches etc.\n\nA higher target "
                  "FPS than your monitor's refresh rate will waste resources, and might give a worse result.";
    } else {
        maxFps = Ship::Context::GetRawInstance()->GetWindow()->GetCurrentRefreshRate();
        tooltip = "Uses Matrix Interpolation to create extra frames, resulting in smoother graphics. This is "
                  "purely visual and does not impact game logic, execution of glitches etc.";
    }
    path.sidebarName = "Graphics";
    AddSidebarEntry("Settings", "Graphics", 3);
    AddWidget(path, "Renderer API (Needs reload)", WIDGET_VIDEO_BACKEND);

    AddWidget(path, "Internal Resolution: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_INTERNAL_RESOLUTION)
        .Callback([](WidgetInfo& info) {
            Ship::Context::GetRawInstance()->GetWindow()->SetResolutionMultiplier(
                CVarGetFloat(CVAR_INTERNAL_RESOLUTION, 1));
        })
        .PreFunc([](WidgetInfo& info) {
            if (mPortMenu->disabledMap.at(DISABLE_FOR_ADVANCED_RESOLUTION_ON).active &&
                mPortMenu->disabledMap.at(DISABLE_FOR_VERTICAL_RES_TOGGLE_ON).active) {
                info.activeDisables.push_back(DISABLE_FOR_ADVANCED_RESOLUTION_ON);
                info.activeDisables.push_back(DISABLE_FOR_VERTICAL_RES_TOGGLE_ON);
            } else if (mPortMenu->disabledMap.at(DISABLE_FOR_LOW_RES_MODE_ON).active) {
                info.activeDisables.push_back(DISABLE_FOR_LOW_RES_MODE_ON);
            }
        })
        .Options(
            FloatSliderOptions()
                .Tooltip("Multiplies your output resolution by the value inputted, as a more intensive but effective "
                         "form of anti-aliasing.")
                .ShowButtons(false)
                .IsPercentage()
                .Format("")
                .Min(0.5f)
                .Max(4.0f));

#ifndef __WIIU__
    AddWidget(path, "Anti-aliasing (MSAA): %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_MSAA_VALUE)
        .Callback([](WidgetInfo& info) {
            Ship::Context::GetRawInstance()->GetWindow()->SetMsaaLevel(CVarGetInteger(CVAR_MSAA_VALUE, 1));
        })
        .Options(
            IntSliderOptions()
                .Tooltip("Activates MSAA (multi-sample anti-aliasing) from 2x up to 8x, to smooth the edges of "
                         "rendered geometry.\n"
                         "Higher sample count will result in smoother edges on models, but may reduce performance.")
                .Min(1)
                .Max(8)
                .DefaultValue(1));
#endif

    AddWidget(path, "Current FPS: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar("gInterpolationFPS")
        .Callback([](WidgetInfo& info) {
            int32_t defaultValue = std::static_pointer_cast<IntSliderOptions>(info.options)->defaultValue;
            if (CVarGetInteger(info.cVar, defaultValue) == defaultValue) {
                info.name = "Current FPS: Original (%d)";
            } else {
                info.name = "Current FPS: %d";
            }
        })
        .PreFunc([](WidgetInfo& info) {
            if (mPortMenu->disabledMap.at(DISABLE_FOR_MATCH_REFRESH_RATE_ON).active)
                info.activeDisables.push_back(DISABLE_FOR_MATCH_REFRESH_RATE_ON);
        })
        .Options(IntSliderOptions().Tooltip(tooltip).Min(30).Max(maxFps).DefaultValue(30));
    AddWidget(path, "Match Refresh Rate", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            int hz = Ship::Context::GetRawInstance()->GetWindow()->GetCurrentRefreshRate();
            if (hz >= 30 && hz <= MAX_FPS) {
                CVarSetInteger("gInterpolationFPS", hz);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
        })
        .PreFunc([](WidgetInfo& info) { info.isHidden = mPortMenu->disabledMap.at(DISABLE_FOR_NOT_DIRECTX).active; })
        .Options(ButtonOptions().Tooltip("Matches interpolation value to the current game's window refresh rate."));
    AddWidget(path, "Match Refresh Rate", WIDGET_CVAR_CHECKBOX)
        .CVar("gMatchRefreshRate")
        .PreFunc([](WidgetInfo& info) { info.isHidden = mPortMenu->disabledMap.at(DISABLE_FOR_DIRECTX).active; })
        .Options(CheckboxOptions().Tooltip("Matches interpolation value to the current game's window refresh rate."));
    AddWidget(path, "Enable Vsync", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_VSYNC_ENABLED)
        .PreFunc([](WidgetInfo& info) { info.isHidden = mPortMenu->disabledMap.at(DISABLE_FOR_NO_VSYNC).active; })
        .Options(CheckboxOptions().Tooltip("Enables Vsync."));
    AddWidget(path, "Windowed Fullscreen", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SDL_WINDOWED_FULLSCREEN)
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = mPortMenu->disabledMap.at(DISABLE_FOR_NO_WINDOWED_FULLSCREEN).active;
        })
        .Options(CheckboxOptions().Tooltip("Enables Windowed Fullscreen Mode."));
    AddWidget(path, "Allow multiple ImGui windows", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENABLE_MULTI_VIEWPORTS)
        .PreFunc(
            [](WidgetInfo& info) { info.isHidden = mPortMenu->disabledMap.at(DISABLE_FOR_NO_MULTI_VIEWPORT).active; })
        .Options(CheckboxOptions()
                     .Tooltip("Allows multiple ImGui windows to be opened at once (Does not effect the game or the splitscreen modes). Requires a reload to take effect.")
                     .DefaultValue(true));
    AddWidget(path, "Texture Filter (Needs reload)", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_TEXTURE_FILTER)
        .Options(ComboboxOptions().Tooltip("Sets the applied Texture Filtering.").ComboMap(textureFilteringMap));

    path.sidebarName = "Controls";
    AddSidebarEntry("Settings", "Controls", 1);
    AddWidget(path,
              "This interface can be a little daunting. Please bear with us as we work to improve the experience "
              "and address some known issues.\n"
              "\n"
              "At first glance, you may notice several input devices displayed below the 'Clear All' button. "
              "Some of these might be other controllers connected to your computer, while others may be "
              "duplicated controllers (a known issue). We recommend clicking on the box with the " ICON_FA_EYE
              " icon and the name of any disconnected or unused controllers to hide their inputs. Make sure the "
              "target controller remains visible.\n"
              "\n"
              "If you encounter issues connecting your controller or registering inputs, try closing Steam or "
              "any other external input software. Alternatively, test a different controller to determine if "
              "it's a compatibility issue.\n",
              WIDGET_TEXT);
    AddWidget(path, "Bindings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Popout Bindings Window", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_CONTROLLER_CONFIGURATION_WINDOW_OPEN)
        .WindowName("Input Editor")
        .Options(ButtonOptions().Tooltip("Enables the separate Bindings Window.").Size(Sizes::Inline));
}
int32_t motionBlurStrength;

void PortMenu::AddEnhancements() {
    AddMenuEntry("Enhancements", "gSettings.Menu.EnhancementsSidebarSection");
    WidgetPath path = { "Enhancements", "General", SECTION_COLUMN_1 };
    AddSidebarEntry("Enhancements", "General", 3);
    AddWidget(path, "No multiplayer feature cuts", WIDGET_CVAR_CHECKBOX)
        .CVar("gMultiplayerNoFeatureCuts")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip("Allows full train and jumbotron in multiplayer, etc."));
    AddWidget(path, "Widescreen portrait spacing", WIDGET_CVAR_CHECKBOX)
        .CVar("gBetterResultPortraits")
        .Options(CheckboxOptions().Tooltip("Alters result portrait spacing for better aesthetics on widescreen"));
    AddWidget(path, "No Level of Detail (LOD)", WIDGET_CVAR_CHECKBOX)
        .CVar("gDisableLod")
        .Options(CheckboxOptions().Tooltip(
            "Disable Level of Detail (LOD) to avoid models using lower poly versions at a distance"));
    AddWidget(path, "Disable Culling", WIDGET_CVAR_CHECKBOX)
        .CVar("gNoCulling")
        .Options(CheckboxOptions().Tooltip("Disable original culling of mk64"));
    AddWidget(path, "Disable Rubberbanding", WIDGET_CVAR_CHECKBOX)
        .CVar("gDisableRubberbanding")
        .Options(CheckboxOptions().Tooltip("Disable rubberbanding in the game."));
    AddWidget(path, "Far Frustrum", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gFarFrustrum")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gNoCulling", 0); })
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(100000.0f)
                     .DefaultValue(100000.0f)
                     .Tooltip("Say how Far the Frustrum are when 'Disable Culling' are enable")
                     .Step(10.0f));
    AddWidget(path, "Enable Custom CC", WIDGET_CVAR_CHECKBOX).CVar("gEnableCustomCC");
    AddWidget(path, "Custom CC", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gCustomCC")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gEnableCustomCC", 0); })
        .Options(FloatSliderOptions().Min(0.0f).Max(1000.0f).DefaultValue(150.0f).Step(10.0f));

    AddWidget(path, "Debug quick HUD race boot", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.DebugQuickBootHudRace")
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("When enabled, boots directly into a random 1P 50cc Grand Prix track as Mario for HUD iteration."));
    AddWidget(path, "Debug telemetry overlay", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.DebugTelemetry.Enabled")
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("Shows the compact on-screen ArcadeKart surface telemetry block."));

    AddWidget(path, "Wheel Steering Tuning: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.WheelSteeringTuning")
        .Options(FloatSliderOptions()
                     .Min(0.25f)
                     .Max(2.0f)
                     .DefaultValue(0.8f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Adjusts the wheel steering curve. Lower values sharpen center response."));
    AddWidget(path, "Wheel Throttle Tuning: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.WheelThrottleTuning")
        .Options(FloatSliderOptions()
                     .Min(0.25f)
                     .Max(2.0f)
                     .DefaultValue(1.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Adjusts the wheel throttle pedal curve."));
    AddWidget(path, "Wheel Brake Tuning: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.WheelBrakeTuning")
        .Options(FloatSliderOptions()
                     .Min(0.25f)
                     .Max(2.0f)
                     .DefaultValue(1.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Adjusts the wheel brake pedal curve."));
    AddWidget(path, "Wheel Clutch Tuning: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.WheelClutchTuning")
        .Options(FloatSliderOptions()
                     .Min(0.25f)
                     .Max(2.0f)
                     .DefaultValue(1.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Adjusts the wheel clutch pedal curve."));
    AddWidget(path, "Wheel spring baseline: %.1fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.LogitechProfilerSpringBaselineMultiplier")
        .Options(FloatSliderOptions()
                     .Min(0.25f)
                     .Max(16.0f)
                     .DefaultValue(8.0f)
                     .Step(0.25f)
                     .Format("%.2f")
                     .Tooltip("Scales the low-speed wheel centering spring baseline."));
    AddWidget(path, "Logitech SDK spring: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.LogitechSdkSpringPercent")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(100.0f)
                     .DefaultValue(96.0f)
                     .Step(5.0f)
                     .Format("%.0f")
                     .Tooltip("Spring force sent through the Logitech Steering Wheel SDK."));
    AddWidget(path, "Use Logitech SDK dynamic spring force", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.LogitechSdkUseSpringForce")
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("Adds the SDK spring force effect on top of preferred spring settings."));
    AddWidget(path, "Use Logitech profiler spring", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.UseLogitechProfilerSpring")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip("Lets the Logitech profiler own the centering spring."));
    AddWidget(path, "Use SDL wheel centering", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.UseSdlWheelCentering")
        .Options(CheckboxOptions().DefaultValue(false).Tooltip("Uses SDL spring/autocenter instead of profiler centering."));
    AddWidget(path, "RPM display multiplier: %.1fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.RpmDisplayMultiplier")
        .Options(FloatSliderOptions()
                     .Min(1.0f)
                     .Max(6.0f)
                     .DefaultValue(4.5f)
                     .Step(0.25f)
                     .Format("%.2f")
                     .Tooltip("Scales displayed RPM without changing transmission physics."));
    AddWidget(path, "HUD safe-zone scale: %.2fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.Hud.SafeZoneScale")
        .Options(FloatSliderOptions()
                     .Min(0.25f)
                     .Max(3.0f)
                     .DefaultValue(1.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Scales ArcadeKart HUD edge insets while keeping cluster layout constants fixed."));
    AddWidget(path, "Reverse drive: %.2fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.ReverseDriveMultiplier")
        .Options(FloatSliderOptions()
                     .Min(0.10f)
                     .Max(1.50f)
                     .DefaultValue(0.75f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Scales throttle torque while manual reverse gear is selected."));
    AddWidget(path, "Reverse speed ratio: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.ReverseSpeedRatio")
        .Options(FloatSliderOptions()
                     .Min(0.05f)
                     .Max(0.70f)
                     .DefaultValue(0.44f)
                     .Step(0.01f)
                     .Format("%.2f")
                     .Tooltip("Reverse gear speed cap as a fraction of the kart's forward top speed."));
    AddWidget(path, "Wheel shifter smoothing: %.0f frames", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.WheelShifterSmoothingFrames")
        .Options(FloatSliderOptions()
                     .Min(1.0f)
                     .Max(8.0f)
                     .DefaultValue(4.0f)
                     .Step(1.0f)
                     .Format("%.0f")
                     .Tooltip("Number of wheel-only shifter samples used for gear smoothing."));
    AddWidget(path, "P1 auto transmission default", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.Transmission.Player1Mode")
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Default transmission mode for player 1; character select changes the current run."));
    AddWidget(path, "P2 auto transmission default", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.Transmission.Player2Mode")
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Default transmission mode for player 2; AI racers always use automatic."));
    AddWidget(path, "P3 auto transmission default", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.Transmission.Player3Mode")
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Default transmission mode for player 3; AI racers always use automatic."));
    AddWidget(path, "P4 auto transmission default", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.Transmission.Player4Mode")
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Default transmission mode for player 4; AI racers always use automatic."));

    AddWidget(path, "Arcade post effects", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.PostFx.Enabled")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip("Applies speed warp, blur, and screen shake."));
    AddWidget(path, "Post FX under HUD", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.PostFx.LayerHud")
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Composites the warped race view below a separate unwarped HUD layer."));
    AddWidget(path, "Post FX tuning sliders only", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.PostFx.TuningSlidersOnly")
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("Uses the shake and warp sliders directly, ignoring speed, boost, and impact drivers."));
    AddWidget(path, "Post FX speed min: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.SpeedMinRatio")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Speed ratio that maps to zero before post-FX response curve shaping."));
    AddWidget(path, "Post FX speed max: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.SpeedMaxRatio")
        .Options(FloatSliderOptions()
                     .Min(0.05f)
                     .Max(2.0f)
                     .DefaultValue(1.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Speed ratio that maps to one before post-FX response curve shaping."));
    AddWidget(path, "Post FX response power: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.ResponsePower")
        .Options(FloatSliderOptions()
                     .Min(0.25f)
                     .Max(4.0f)
                     .DefaultValue(2.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Exponential response curve applied to normalized post-FX drivers."));
    AddWidget(path, "Post FX shake response power: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.ShakeResponsePower")
        .Options(FloatSliderOptions()
                     .Min(0.25f)
                     .Max(6.0f)
                     .DefaultValue(3.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Exponential response curve applied only to screen-shake drivers."));
    AddWidget(path, "Speed camera wide FOV: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.Camera.SpeedWideFov")
        .Options(FloatSliderOptions()
                     .Min(40.0f)
                     .Max(120.0f)
                     .DefaultValue(100.0f)
                     .Step(1.0f)
                     .Format("%.0f")
                     .Tooltip("Camera FOV at the low end of the shared speed response curve."));
    AddWidget(path, "Speed camera narrow FOV: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.Camera.SpeedNarrowFov")
        .Options(FloatSliderOptions()
                     .Min(40.0f)
                     .Max(120.0f)
                     .DefaultValue(60.0f)
                     .Step(1.0f)
                     .Format("%.0f")
                     .Tooltip("Camera FOV at the high end of the shared speed response curve."));
    AddWidget(path, "Speed camera position shift", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.Camera.SpeedPositionShift")
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Moves the chase camera down and back using the shared speed response curve."));
    AddWidget(path, "Speed camera near back: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.Camera.PositionNearBack")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(120.0f)
                     .DefaultValue(45.0f)
                     .Step(0.5f)
                     .Format("%.1f")
                     .Tooltip("Camera distance behind the kart at zero speed."));
    AddWidget(path, "Speed camera near up: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.Camera.PositionNearUp")
        .Options(FloatSliderOptions()
                     .Min(-20.0f)
                     .Max(40.0f)
                     .DefaultValue(12.0f)
                     .Step(0.5f)
                     .Format("%.1f")
                     .Tooltip("Camera height above the kart at zero speed."));
    AddWidget(path, "Speed camera far back: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.Camera.PositionFarBack")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(120.0f)
                     .DefaultValue(60.0f)
                     .Step(0.5f)
                     .Format("%.1f")
                     .Tooltip("Camera distance behind the kart at full speed."));
    AddWidget(path, "Speed camera far up: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.Camera.PositionFarUp")
        .Options(FloatSliderOptions()
                     .Min(-20.0f)
                     .Max(40.0f)
                     .DefaultValue(3.0f)
                     .Step(0.5f)
                     .Format("%.1f")
                     .Tooltip("Camera height above the kart at full speed."));
    AddWidget(path, "Post FX test shake: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.TestShakeSlider")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Normalized screen-shake test value used while slider-only tuning is enabled."));
    AddWidget(path, "Post FX test warp: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.TestWarpSlider")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Normalized fisheye/barrel test value used while slider-only tuning is enabled."));
    AddWidget(path, "Post FX test shake raw max: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.TuningShakeInputMax")
        .Options(FloatSliderOptions()
                     .Min(0.01f)
                     .Max(1.0f)
                     .DefaultValue(0.25f)
                     .Step(0.01f)
                     .Format("%.2f")
                     .Tooltip("Old raw shake value represented by a 100% normalized test shake slider."));
    AddWidget(path, "Post FX test warp raw max: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.TuningWarpInputMax")
        .Options(FloatSliderOptions()
                     .Min(0.01f)
                     .Max(3.0f)
                     .DefaultValue(2.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Old raw warp value represented by a 100% normalized test warp slider."));
    AddWidget(path, "Manual post FX intensity", WIDGET_CVAR_CHECKBOX)
        .CVar("gArcadeKart.PostFx.ManualOverride")
        .Options(CheckboxOptions()
                     .DefaultValue(false)
                     .Tooltip("Locks post effects to the manual intensity instead of race speed and impacts."));
    AddWidget(path, "Post FX manual intensity: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.ManualIntensity")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.0f)
                     .Step(0.10f)
                     .Format("%.2f")
                     .Tooltip("Locks all post effects to this value when manual post FX intensity is enabled."));
    AddWidget(path, "Post FX overscan: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.OverscanPercent")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(0.35f)
                     .DefaultValue(0.15f)
                     .Step(0.01f)
                     .Format("%.2f")
                     .Tooltip("Extra framebuffer overdraw used by the warp effect."));
    AddWidget(path, "HUD overscan compensation: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.HudOverscanCompensation")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(1.5f)
                     .DefaultValue(1.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Reserved for HUD layout compensation while post effects render under the HUD."));
    AddWidget(path, "Post FX barrel: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.BarrelStrength")
        .Options(FloatSliderOptions().Min(0.0f).Max(0.35f).DefaultValue(0.035f).Step(0.005f).Format("%.3f"));
    AddWidget(path, "Post FX speed warp: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.SpeedBarrelStrength")
        .Options(FloatSliderOptions().Min(0.0f).Max(0.5f).DefaultValue(0.085f).Step(0.005f).Format("%.3f"));
    AddWidget(path, "Post FX boost warp: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.BoostBarrelStrength")
        .Options(FloatSliderOptions().Min(0.0f).Max(0.65f).DefaultValue(0.12f).Step(0.005f).Format("%.3f"));
    AddWidget(path, "Post FX warp intensity: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.WarpIntensity")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(3.0f)
                     .DefaultValue(1.0f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Normal-mode fisheye/barrel multiplier used after slider-only tuning is off."));
    AddWidget(path, "Post FX warp output scale: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.WarpOutputScale")
        .Options(FloatSliderOptions()
                      .Min(0.0f)
                      .Max(8.0f)
                      .DefaultValue(4.0f)
                      .Step(0.05f)
                      .Format("%.2f")
                      .Tooltip("Final fisheye/barrel multiplier. 4.0 doubles the previous warp output."));
    AddWidget(path, "Post FX tuning warp strength: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.TuningWarpStrength")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.16f)
                     .Step(0.01f)
                     .Format("%.2f")
                     .Tooltip("Visual barrel strength per warp-intensity unit in slider-only tuning mode."));
    AddWidget(path, "Post FX tuning shake strength: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.TuningShakeStrength")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(0.1f)
                     .DefaultValue(0.04f)
                     .Step(0.002f)
                     .Format("%.3f")
                     .Tooltip("Screen-shake strength at 100% test shake in slider-only tuning mode."));
    AddWidget(path, "Post FX motion blur: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.MotionBlur")
        .Options(FloatSliderOptions().Min(0.0f).Max(1.0f).DefaultValue(0.28f).Step(0.02f).Format("%.2f"));
    AddWidget(path, "Post FX shake: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.ShakeStrength")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(0.1f)
                     .DefaultValue(0.018f)
                     .Step(0.002f)
                     .Format("%.3f")
                     .Tooltip("Normal-mode impact screen-shake strength used after slider-only tuning is off."));
    AddWidget(path, "Post FX shake output scale: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.ShakeOutputScale")
        .Options(FloatSliderOptions()
                     .Min(0.0f)
                     .Max(2.0f)
                     .DefaultValue(0.3f)
                     .Step(0.05f)
                     .Format("%.2f")
                     .Tooltip("Final screen-shake multiplier. 0.3 is 60% of the previous maximum."));
    AddWidget(path, "Post FX shake fast frequency: %.0f Hz", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.ShakeFrequencyFastHz")
        .Options(FloatSliderOptions()
                      .Min(0.0f)
                      .Max(120.0f)
                      .DefaultValue(60.0f)
                      .Step(1.0f)
                      .Format("%.0f")
                      .Tooltip("Screen-shake frequency on smooth surfaces."));
    AddWidget(path, "Post FX shake slow frequency: %.0f Hz", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.ShakeFrequencySlowHz")
        .Options(FloatSliderOptions()
                      .Min(0.0f)
                      .Max(120.0f)
                      .DefaultValue(8.0f)
                      .Step(1.0f)
                      .Format("%.0f")
                      .Tooltip("Screen-shake frequency on maximum roughness."));
    AddWidget(path, "Post FX shake frequency roughness curve: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gArcadeKart.PostFx.ShakeFrequencyRoughnessPower")
        .Options(FloatSliderOptions()
                      .Min(0.01f)
                      .Max(8.0f)
                      .DefaultValue(0.5f)
                      .Step(0.05f)
                      .Format("%.2f")
                      .Tooltip("Exponent applied to inverse surface roughness before choosing shake frequency."));
    AddWidget(path, "Enable Digital Speedometer", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnableDigitalSpeedometer")
        .Options(CheckboxOptions().Tooltip("Welcome to the modern era"));

    AddWidget(path, "Harder CPU", WIDGET_CVAR_CHECKBOX).CVar("gHarderCPU");

    AddWidget(path, "Show Spaghetti version", WIDGET_CVAR_CHECKBOX)
        .CVar("gShowSpaghettiVersion")
        .Options(CheckboxOptions().Tooltip("Show the Spaghetti Kart version on the Mario Kart menu").DefaultValue(true));

    // Look behind is native and on by default (hold D-pad Down or Y/Triangle). This checkbox
    // is the OFF switch for players who want the button back - checking it disables look behind.
    AddWidget(path, "Disable Look Behind Camera", WIDGET_CVAR_CHECKBOX)
        .CVar("gNoLookBehind")
        .Options(CheckboxOptions()
                     .Tooltip("Look behind (hold D-pad Down or Y/Triangle) is on by default. Check "
                              "this to turn it off and free the button."));
    AddWidget(path, "Fix Visuals", WIDGET_CVAR_CHECKBOX)
        .CVar("gFixVisuals")
        .Options(CheckboxOptions().Tooltip("Fixes the second last lamp glow in Banshee Boardwalk"));

    AddRulesets();

    path = { "Enhancements", "Cheats", SECTION_COLUMN_1 };
    AddSidebarEntry("Enhancements", "Cheats", 3);
    AddWidget(path, "Moon Jump", WIDGET_CVAR_CHECKBOX).CVar("gEnableMoonJump");
    AddWidget(path, "Disable Wall Collision", WIDGET_CVAR_CHECKBOX)
        .CVar("gNoWallColision")
        .Options(CheckboxOptions().Tooltip("Disable wall collision."));
    AddWidget(path, "Min Height", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gMinHeight")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gNoWallColision", 0); })
        .Options(FloatSliderOptions().Min(-50.0f).Max(50.0f).DefaultValue(0.0f).Tooltip(
            "When Disable Wall Collision are enable what is the minimal height you can get."));
}

#ifdef __SWITCH__
static const std::unordered_map<int32_t, const char*> switchCPUProfiles = {
    { Ship::SwitchProfiles::MAXIMUM, "Maximum Performance" },
    { Ship::SwitchProfiles::HIGH, "High Performance" },
    { Ship::SwitchProfiles::BOOST, "Boost Performance" },
    { Ship::SwitchProfiles::STOCK, "Stock Performance" },
    { Ship::SwitchProfiles::POWERSAVINGM1, "Powersaving Mode 1" },
    { Ship::SwitchProfiles::POWERSAVINGM2, "Powersaving Mode 2" },
    { Ship::SwitchProfiles::POWERSAVINGM3, "Powersaving Mode 3" }
};
#endif

void PortMenu::AddRulesets() {
    WidgetPath path = { "Enhancements", "Rulesets", SECTION_COLUMN_1 };
    AddSidebarEntry("Enhancements", "Rulesets", 3);

    // Requires more testing
    // AddWidget(path, "Number of Laps", WIDGET_CVAR_SLIDER_INT)
    //     .CVar("gNumLaps")
    //     .Options(UIWidgets::IntSliderOptions().Min().Max(20).Step(1).DefaultValue(3));
    AddWidget(path, "Unique Character Selections", WIDGET_CVAR_CHECKBOX)
        .CVar("gUniqueCharacterSelections")
        .Options(CheckboxOptions()
          .Tooltip(
            "Prevents players from selecting the same character")
          .DefaultValue(true)
        );
    AddWidget(path, "No Itemboxes", WIDGET_CVAR_CHECKBOX)
        .CVar("gDisableItemboxes")
        .Options(CheckboxOptions().Tooltip(
            "Prevents Itemboxes from spawning"));
    AddWidget(path, "All Thwomps are Marty", WIDGET_CVAR_CHECKBOX)
        .CVar("gAllThwompsAreMarty")
        .Options(CheckboxOptions().Tooltip(
            "All Thwomps are Marty"));
    AddWidget(path, "All Bomb Karts in Chase Mode", WIDGET_CVAR_CHECKBOX)
        .CVar("gAllBombKartsChase")
        .Options(CheckboxOptions().Tooltip(
            "These karts will chase you!!!"));
    AddWidget(path, "Get the trophies!", WIDGET_CVAR_CHECKBOX)
        .CVar("gGoFish")
        .Options(CheckboxOptions().Tooltip(
            "Collect as many trophies as you can. Racer with the most trophies wins!"));
    AddWidget(path, "Shells Shoot Straight", WIDGET_CVAR_CHECKBOX)
        .CVar("gShellsShootStraight")
        .Options(CheckboxOptions().Tooltip("Fixes a logic bug in the game code. No evidence if this is intended gameplay or not."));
    AddWidget(path, "Track X Stretch", WIDGET_SLIDER_FLOAT)
        .ValuePointer(&gVtxStretch[0])
        .Options(UIWidgets::FloatSliderOptions().Min(0.1f).Max(10.0f).Step(0.1f).Format("%.2f"));
    AddWidget(path, "Track Y Stretch", WIDGET_SLIDER_FLOAT)
        .ValuePointer(&gVtxStretch[1])
        .Options(UIWidgets::FloatSliderOptions().Min(0.1f).Max(10.0f).Step(0.1f).Format("%.2f"));
    AddWidget(path, "Track Z Stretch", WIDGET_SLIDER_FLOAT)
        .ValuePointer(&gVtxStretch[2])
        .Options(UIWidgets::FloatSliderOptions().Min(0.1f).Max(10.0f).Step(0.1f).Format("%.2f"));


    AddWidget(path, "Trains", WIDGET_CVAR_SLIDER_INT)
        .CVar("gNumTrains")
        .Options(UIWidgets::IntSliderOptions().Min(0).Max(19).Step(1).DefaultValue(2));
    AddWidget(path, "Carriages", WIDGET_CVAR_SLIDER_INT)
        .CVar("gNumCarriages")
        .Options(UIWidgets::IntSliderOptions().Min(0).Max(74).Step(1).DefaultValue(5));
    AddWidget(path, "Train has a tender", WIDGET_CVAR_CHECKBOX)
        .CVar("gHasTender")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(1)
        .Tooltip("This option is only valid if there are no carriages on the train"));

    AddWidget(path, "Trucks", WIDGET_CVAR_SLIDER_INT)
        .CVar("gNumTrucks")
        .Options(UIWidgets::IntSliderOptions().Min(0).Max(50).Step(1).DefaultValue(7));
    AddWidget(path, "Buses", WIDGET_CVAR_SLIDER_INT)
        .CVar("gNumBuses")
        .Options(UIWidgets::IntSliderOptions().Min(0).Max(50).Step(1).DefaultValue(7));
    AddWidget(path, "Tanker Trucks", WIDGET_CVAR_SLIDER_INT)
        .CVar("gNumTankerTrucks")
        .Options(UIWidgets::IntSliderOptions().Min(0).Max(50).Step(1).DefaultValue(7));
    AddWidget(path, "Cars", WIDGET_CVAR_SLIDER_INT)
        .CVar("gNumCars")
        .Options(UIWidgets::IntSliderOptions().Min(0).Max(50).Step(1).DefaultValue(7));

    // Flatscreen view modes. The chase-cam override (camera.c func_8001E45C) reads these CVars
    // every frame, so changes apply live mid-race. VR ignores them - it has its own View Mode.
    AddSidebarEntry("Enhancements", "Camera", 1);
    path = { "Enhancements", "Camera", SECTION_COLUMN_1 };
    static const std::unordered_map<int32_t, const char*> flatViewModeOptions = {
        { 0, "Third Person" },
        { 1, "First Person" },
        { 2, "Diorama" },
    };
    AddWidget(path, "Flatscreen View Mode", WIDGET_CVAR_COMBOBOX)
        .CVar("gFlatViewMode")
        .Options(UIWidgets::ComboboxOptions()
                     .ComboMap(flatViewModeOptions)
                     .DefaultIndex(0)
                     .Tooltip("How the game is framed when playing WITHOUT a headset. Third Person = the classic "
                              "chase cam; First Person = ride at the driver's head (your own kart hides); "
                              "Diorama = a high tabletop view looking down on the action. VR has its own View "
                              "Mode and ignores this."));
    AddWidget(path, "First Person Eye Height", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gFlatFPHeight")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gFlatViewMode", 0) != 1); })
        .Options(UIWidgets::FloatSliderOptions().Min(1.0f).Max(40.0f).Step(0.5f).DefaultValue(8.0f).Format("%.1f")
                     .Tooltip("How high above the kart the first person eye sits, in game units. Around 8 is "
                              "driver-head height; raise it for a hood-cam feel."));
    AddWidget(path, "Diorama Height", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gFlatDioramaHeight")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gFlatViewMode", 0) != 2); })
        .Options(UIWidgets::FloatSliderOptions().Min(30.0f).Max(900.0f).Step(10.0f).DefaultValue(50.0f).Format("%.0f")
                     .Tooltip("How high the diorama camera floats above the kart. Higher = more top-down, more "
                              "tabletop."));
    AddWidget(path, "Diorama Distance", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gFlatDioramaDist")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gFlatViewMode", 0) != 2); })
        .Options(UIWidgets::FloatSliderOptions().Min(0.0f).Max(700.0f).Step(10.0f).DefaultValue(190.0f).Format("%.0f")
                     .Tooltip("How far the diorama camera trails behind the kart. 0 = straight overhead."));
    AddWidget(path, "Reset Camera Settings", WIDGET_BUTTON)
        .Callback([](WidgetInfo&) {
            CVarSetFloat("gFlatFPHeight", 8.0f);
            CVarSetFloat("gFlatDioramaHeight", 50.0f);
            CVarSetFloat("gFlatDioramaDist", 190.0f);
        });

    // VR options. The VR layer reads these CVars every frame, so changes apply live. Shown on the
    // desktop window; in VR the desktop ImGui is visible on the monitor (e.g. via Virtual Desktop).
    AddSidebarEntry("Enhancements", "VR", 1);
    path = { "Enhancements", "VR", SECTION_COLUMN_1 };
    static const std::unordered_map<int32_t, const char*> vrViewModeOptions = {
        { 0, "Third Person" },
        { 1, "First Person" },
        { 2, "Theater" },
        { 3, "Diorama" },
    };
    AddWidget(path, "View Mode", WIDGET_CVAR_COMBOBOX)
        .CVar("gVRViewMode")
        .Options(UIWidgets::ComboboxOptions()
                     .ComboMap(vrViewModeOptions)
                     .DefaultIndex(0)
                     .Tooltip("How the game is shown in VR. Third Person = the classic chase cam in stereo 3D; "
                              "First Person = ride in the kart; Theater = flat screen floating in front of you "
                              "(most comfortable); Diorama = the track shrunk to a tabletop you lean around."));
    AddWidget(path, "First Person Forward (m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRFirstPersonFwd")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gVRViewMode", 0) != 1); })
        .Options(UIWidgets::FloatSliderOptions().Min(-15.0f).Max(15.0f).Step(0.25f).DefaultValue(3.0f).Format("%.2f")
                     .Tooltip("How far the eye is pushed toward the kart in First Person. It rides the chase "
                              "cam's slightly downward angle, so large values dive toward the floor - raise it a "
                              "little at a time. If it moves the wrong way, use a negative value."));
    AddWidget(path, "First Person World Scale (units/m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRFirstPersonScale")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gVRViewMode", 0) != 1); })
        .Options(UIWidgets::FloatSliderOptions().Min(5.0f).Max(500.0f).Step(5.0f).DefaultValue(20.0f).Format("%.0f")
                     .Tooltip("How big the world feels in First Person. Separate from the main World Scale so it "
                              "won't change Third Person. Lower = larger world."));
    AddWidget(path, "First Person Eye Height (m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRFirstPersonEyeHeight")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gVRViewMode", 0) != 1); })
        .Options(UIWidgets::FloatSliderOptions().Min(-1.0f).Max(1.0f).Step(0.02f).DefaultValue(0.0f).Format("%.2f")
                     .Tooltip("Raise or lower the eye in First Person. Separate from the Third Person Eye Height."));
    AddWidget(path, "Diorama Distance (m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRDioramaDist")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gVRViewMode", 0) != 3); })
        .Options(UIWidgets::FloatSliderOptions().Min(0.1f).Max(3.0f).Step(0.05f).DefaultValue(0.30f).Format("%.2f")
                     .Tooltip("How far in front of you the tabletop sits (Diorama mode)."));
    AddWidget(path, "Diorama World Scale (units/m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRDioramaWorldScale")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gVRViewMode", 0) != 3); })
        .Options(UIWidgets::FloatSliderOptions().Min(20.0f).Max(2000.0f).Step(5.0f).DefaultValue(115.0f).Format("%.0f")
                     .Tooltip("Size of the tabletop world. This is separate from the main World Scale, so it "
                              "won't change Third/First Person. Higher = smaller tabletop."));
    AddWidget(path, "Diorama Height (m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRDioramaHeight")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gVRViewMode", 0) != 3); })
        .Options(UIWidgets::FloatSliderOptions().Min(-1.5f).Max(1.0f).Step(0.05f).DefaultValue(-0.15f).Format("%.2f")
                     .Tooltip("Raise or lower the tabletop to a comfortable height (Diorama mode)."));
    AddWidget(path, "World Scale (units/m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRWorldScale")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gVRViewMode", 0) != 0); })
        .Options(UIWidgets::FloatSliderOptions().Min(5.0f).Max(2000.0f).Step(5.0f).DefaultValue(20.0f).Format("%.0f")
                     .Tooltip("How big the world feels in Third Person. Smaller value = larger world. "
                              "First Person and Diorama have their own scale knobs."));
    AddWidget(path, "Third Person Distance (m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRThirdPersonDist")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gVRViewMode", 0) != 0); })
        .Options(UIWidgets::FloatSliderOptions().Min(-15.0f).Max(50.0f).Step(0.5f).DefaultValue(0.0f).Format("%.1f")
                     .Tooltip("Move the Third Person camera further behind the kart (and higher) with positive "
                              "values, or closer with negative. 0 = the game's stock chase distance."));
    AddWidget(path, "Stereo Depth", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRStereo")
        .Options(UIWidgets::FloatSliderOptions().Min(0.0f).Max(1.5f).Step(0.05f).DefaultValue(0.5f).Format("%.2f")
                     .Tooltip("Stereo separation strength. Lower is gentler / less eye strain."));
    AddWidget(path, "Eye Height (m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVREyeHeight")
        .PreFunc([](WidgetInfo& info) { info.isHidden = (CVarGetInteger("gVRViewMode", 0) != 0); })
        .Options(UIWidgets::FloatSliderOptions().Min(-1.0f).Max(1.0f).Step(0.02f).DefaultValue(0.16f).Format("%.2f")
                     .Tooltip("Raise or lower the eye in Third Person. First Person has its own eye height."));
    AddWidget(path, "HUD Size", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRHudScale")
        .Options(UIWidgets::FloatSliderOptions().Min(0.2f).Max(1.2f).Step(0.05f).DefaultValue(0.35f).Format("%.2f")
                     .Tooltip("How much of your view the in-game HUD (item box, positions) fills. Smaller = more central."));
    AddWidget(path, "HUD Distance (m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRHudDist")
        .Options(UIWidgets::FloatSliderOptions().Min(0.5f).Max(8.0f).Step(0.1f).DefaultValue(2.90f).Format("%.1f"));
    AddWidget(path, "Menu Distance (m)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRMenuDist")
        .Options(UIWidgets::FloatSliderOptions().Min(1.0f).Max(8.0f).Step(0.1f).DefaultValue(3.2f).Format("%.1f"));
    AddWidget(path, "Menu Size", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRMenuSize")
        .Options(UIWidgets::FloatSliderOptions().Min(2.0f).Max(12.0f).Step(0.2f).DefaultValue(4.2f).Format("%.1f"));
    AddWidget(path, "Menu Opacity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVRMenuOpacity")
        .Options(UIWidgets::FloatSliderOptions().Min(0.3f).Max(1.0f).Step(0.05f).DefaultValue(0.5f).Format("%.2f")
                     .Tooltip("How solid the VR menu panel is. 1.0 = opaque; lower lets you see the game "
                              "through the menu. Won't go fully transparent so the menu stays usable."));
    AddWidget(path, "Hide HUD (keep item box)", WIDGET_CVAR_CHECKBOX)
        .CVar("gVRHideHud")
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Hide the lap counter, race timer, minimap, positions and rankings. The item box stays visible "
            "so you can still see your held item."));
    AddWidget(path, "World Locked HUD", WIDGET_CVAR_CHECKBOX)
        .CVar("gVRHudWorldLock")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));
    AddWidget(path, "Flip Cam (FP)", WIDGET_CVAR_CHECKBOX)
        .CVar("gVRFPDrama")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true).Tooltip(
            "Simulate spins, tumbles, bomb launches and lightning strikes from the driver's seat - the "
            "view rotates with the kart through the whole animation."));
    AddWidget(path, "Ease Back on Hit (FP)", WIDGET_CVAR_CHECKBOX)
        .CVar("gVRFlipCam")
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Pull the camera back to the chase view while your kart takes a hit, catches big air or "
            "hangs from Lakitu, so you watch it from outside, then ease back in. Takes priority over "
            "the Flip Cam simulation while active."));
    AddWidget(path, "Reset VR Settings", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            CVarSetInteger("gVRViewMode", 0);
            CVarSetFloat("gVRFirstPersonFwd", 3.0f);
            CVarSetFloat("gVRFirstPersonScale", 20.0f);
            CVarSetFloat("gVRFirstPersonEyeHeight", 0.0f);
            CVarSetFloat("gVRThirdPersonDist", 0.0f);
            CVarSetFloat("gVRDioramaDist", 0.30f);
            CVarSetFloat("gVRDioramaWorldScale", 115.0f);
            CVarSetFloat("gVRDioramaHeight", -0.15f);
            CVarSetFloat("gVRWorldScale", 20.0f);
            CVarSetFloat("gVRStereo", 0.5f);
            CVarSetFloat("gVREyeHeight", 0.16f);
            CVarSetFloat("gVRHudScale", 0.35f);
            CVarSetFloat("gVRHudDist", 2.90f);
            CVarSetFloat("gVRMenuDist", 3.2f);
            CVarSetFloat("gVRMenuSize", 4.2f);
            CVarSetFloat("gVRMenuOpacity", 1.0f);
            CVarSetInteger("gVRHideHud", 0);
            CVarSetInteger("gVRHudWorldLock", 1);
            CVarSetInteger("gVRFlipCam", 0);
            CVarSetInteger("gVRFPDrama", 1);
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ButtonOptions().Tooltip("Reset all VR settings on this page to their defaults."));
}

void PortMenu::AddDevTools() {
    AddMenuEntry("Developer", "gSettings.Menu.DevToolsSidebarSection");
    AddSidebarEntry("Developer", "General", 3);
    WidgetPath path = { "Developer", "General", SECTION_COLUMN_1 };
#ifdef __SWITCH__
    AddWidget(path, "Switch CPU Profile", WIDGET_CVAR_COMBOBOX)
        .CVar("gSwitchPerfMode")
        .Options(ComboboxOptions()
                     .Tooltip("Switches the CPU profile to a different one")
                     .ComboMap(switchCPUProfiles)
                     .DefaultIndex(Ship::SwitchProfiles::STOCK))
        .Callback([](WidgetInfo& info) { Ship::Switch::ApplyOverclock(); });
#endif
    AddWidget(path, "Popout Menu", WIDGET_CVAR_CHECKBOX)
        .CVar("gSettings.Menu.Popout")
        .Options(CheckboxOptions().Tooltip("Changes the menu display from overlay to windowed."));
    AddWidget(path, "Debug Mode", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnableDebugMode")
        .Callback([](WidgetInfo& info) {
            SPDLOG_INFO(CVarGetInteger("gEnableDebugMode", 0) == 0 ? "Debug Mode deactivated" : "Debug Mode activated");
        })
        .Options(CheckboxOptions().Tooltip("Enables Debug Mode."));
    AddWidget(path, "Modify Interpolation Target FPS", WIDGET_CVAR_CHECKBOX)
        .CVar("gModifyInterpolationTargetFPS")
        .Options(CheckboxOptions().Tooltip("For testing frame interpolation."));
    AddWidget(path, "Interpolation Target FPS", WIDGET_CVAR_SLIDER_INT)
        .CVar("gInterpolationTargetFPS")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gModifyInterpolationTargetFPS", 0); })
        .Options(IntSliderOptions()
                     .Tooltip("Sets the target FPS for interpolation. When Modify Interpolation Target FPS are enable")
                     .Min(15)
                     .Max(MAX_FPS)
                     .DefaultValue(60));
    AddWidget(path, "Render Collision", WIDGET_CVAR_CHECKBOX)
        .CVar("gRenderCollisionMesh")
        .Options(CheckboxOptions().Tooltip("Draws the collision mesh instead of the track mesh"));

    AddSceneVisibility();

    path = { "Developer", "Gfx Debugger", SECTION_COLUMN_1 };
    AddSidebarEntry("Developer", "Gfx Debugger", 1);
    AddWidget(path, "Popout Gfx Debugger", WIDGET_WINDOW_BUTTON)
        .CVar("gGfxDebuggerEnabled")
        .Options(ButtonOptions().Tooltip(
            "Enables the Gfx Debugger window, allowing you to input commands. Type help for some examples"))
        .WindowName("GfxDebuggerWindow");

    path = { "Developer", "Stats", SECTION_COLUMN_1 };
    AddSidebarEntry("Developer", "Stats", 1);
    AddWidget(path, "Popout Stats", WIDGET_WINDOW_BUTTON)
        .CVar("gStatsEnabled")
        .Options(ButtonOptions().Tooltip(
            "Shows the stats window, with your FPS and frametimes, and the OS you're playing on"))
        .WindowName("Stats");

    path = { "Developer", "Console", SECTION_COLUMN_1 };
    AddSidebarEntry("Developer", "Console", 1);
    AddWidget(path, "Popout Console", WIDGET_WINDOW_BUTTON)
        .CVar("gConsoleEnabled")
        .Options(ButtonOptions().Tooltip(
            "Enables the console window, allowing you to input commands. Type help for some examples"))
        .WindowName("Console");
}

void PortMenu::AddSceneVisibility() {
    WidgetPath path = { "Developer", "Scene Visibility", SECTION_COLUMN_1 };
    AddSidebarEntry("Developer", "Scene Visibility", 1);

    AddWidget(path, "Draw Sky", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawSky")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Sky Actors", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawSkyActors")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Track Geometry", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawTrackGeometry")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Transparent Track", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawTransparentTrack")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Players", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawPlayers")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Item Boxes", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawItemBoxes")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Original Actors", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawCActors")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Rewrite Actors", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawCPPActors")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Static Mesh", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawStaticMeshActors")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Objects", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawObjects")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw Particles", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawParticles")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));

    AddWidget(path, "Draw HUD", WIDGET_CVAR_CHECKBOX)
        .CVar("gDrawHUD")
        .Options(UIWidgets::CheckboxOptions().DefaultValue(true));
}

PortMenu::PortMenu(const std::string& consoleVariable, const std::string& name)
    : Menu(consoleVariable, name, 0, UIWidgets::Colors::LightBlue) {
}

void PortMenu::InitElement() {
    Ship::Menu::InitElement();
    AddSettings();
    AddEnhancements();
    AddDevTools();

    if (CVarGetInteger("gSettings.Menu.SidebarSearch", 0)) {
        InsertSidebarSearch();
    }

    for (auto& initFunc : MenuInit::GetInitFuncs()) {
        initFunc();
    }

    disabledMap = {
        { DISABLE_FOR_FREE_CAM_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger("gFreecam", 0); }, "Freecam is Enabled" } },
        { DISABLE_FOR_FREE_CAM_OFF,
          { [](disabledInfo& info) -> bool { return !CVarGetInteger("gFreecam", 0); }, "Freecam is Disabled" } },
        { DISABLE_FOR_DEBUG_MODE_OFF,
          { [](disabledInfo& info) -> bool { return !CVarGetInteger("gEnableDebugMode", 0); },
            "Debug Mode is Disabled" } },
        { DISABLE_FOR_NO_VSYNC,
          { [](disabledInfo& info) -> bool {
               return !Ship::Context::GetRawInstance()->GetWindow()->CanDisableVerticalSync();
           },
            "Disabling VSync not supported" } },
        { DISABLE_FOR_NO_WINDOWED_FULLSCREEN,
          { [](disabledInfo& info) -> bool {
               return !Ship::Context::GetRawInstance()->GetWindow()->SupportsWindowedFullscreen();
           },
            "Windowed Fullscreen not supported" } },
        { DISABLE_FOR_NO_MULTI_VIEWPORT,
          { [](disabledInfo& info) -> bool {
               return !Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SupportsViewports();
           },
            "Multi-viewports not supported" } },
        { DISABLE_FOR_NOT_DIRECTX,
          { [](disabledInfo& info) -> bool {
               return Ship::Context::GetRawInstance()->GetWindow()->GetWindowBackend() !=
                      Fast::WindowBackend::FAST3D_DXGI_DX11;
           },
            "Available Only on DirectX" } },
        { DISABLE_FOR_DIRECTX,
          { [](disabledInfo& info) -> bool {
               return Ship::Context::GetRawInstance()->GetWindow()->GetWindowBackend() ==
                      Fast::WindowBackend::FAST3D_DXGI_DX11;
           },
            "Not Available on DirectX" } },
        { DISABLE_FOR_MATCH_REFRESH_RATE_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger("gMatchRefreshRate", 0); },
            "Match Refresh Rate is Enabled" } },
        { DISABLE_FOR_ADVANCED_RESOLUTION_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".Enabled", 0); },
            "Advanced Resolution Enabled" } },
        { DISABLE_FOR_VERTICAL_RES_TOGGLE_ON,
          { [](disabledInfo& info) -> bool {
               return CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".VerticalResolutionToggle", 0);
           },
            "Vertical Resolution Toggle Enabled" } },
        { DISABLE_FOR_LOW_RES_MODE_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger(CVAR_LOW_RES_MODE, 0); }, "N64 Mode Enabled" } },
        //{ DISABLE_FOR_MULTIPLAYER_CONNECTED,
        //  { CheckNetworkConnected, "Multiplayer Connected"}},
    };
}

void PortMenu::UpdateElement() {
    Ship::Menu::UpdateElement();
}

void PortMenu::VisitOptions(const std::function<void(const std::string&, const std::string&, WidgetInfo&)>& visitor) {
    for (auto& [reason, info] : disabledMap) info.active = info.evaluation(info);
    for (const auto& section : menuOrder) {
        auto& entry = menuEntries.at(section);
        for (const auto& sidebar : entry.sidebarOrder) {
            for (auto& column : entry.sidebars.at(sidebar).columnWidgets) {
                for (auto& widget : column) visitor(section, sidebar, widget);
            }
        }
    }
}

void PortMenu::Draw() {
    Ship::Menu::Draw();
}

void PortMenu::DrawElement() {
    Ship::Menu::DrawElement();
}
} // namespace GameUI
