#include "GameOptionsModel.h"
#include "PortMenu.h"
#include "ImguiUI.h"
#include "game_options.h"
#include "port/net/NetRace.h"
#include "ship/window/Window.h"
#include "ship/window/gui/Gui.h"
#include <cstdio>
#include <cstring>
#include <optional>

extern "C" {
#include "main.h"
#include "menus.h"
#include "menu_items.h"
#include "menu_pointer.h"
#include "race_mods_menu.h"
#include "kart_input.h"
#include "save.h"
#include "save_data.h"
#include "audio/external.h"
#include <sounds.h>
#include "port/vr/vr.h"
void func_8028DF38(void);
void reset_cycle_flash_menu(void);
}

namespace GameUI { extern std::shared_ptr<PortMenu> mPortMenu; }

namespace {
using GameUI::Options::Page;
using GameUI::Options::Cursor;
using GameUI::Options::FontText;
struct Frame { Page page; Cursor cursor; };
enum class Action {
    None, Resume, Restart, RaceSetup, Course, Driver, Quit, SoundMode,
    CopyGhost, EraseSave, Host, Join, StartLan, LeaveLan, Character, Port,
    Octet0, Octet1, Octet2, Octet3
};
struct Row {
    std::string label;
    WidgetInfo* widget = nullptr;
    std::optional<Page> destination;
    Action action = Action::None;
    bool enabled = true;
};
std::vector<Frame> stack;
std::vector<Row> rows;
size_t layoutHash = 0;
bool fromPause = false, fromStockOptions = false, requested = false, backRequested = false;
bool legacy = false;
bool bindingsPopup = false;
bool liveNetwork = false;
Controller networkController{};
int heldDirection = 0, holdFrames = 0;
int legacyRequest = 0, pageScroll = 0, generation = 0;
std::string notice;
constexpr int Back = 1000, Done = 1001, Previous = 1002, Next = 1003;

const char* PageTitle(Page page) {
    switch (page) {
        case Page::Pause: return "PAUSED";
        case Page::Root: return "OPTIONS";
        case Page::General: return "GENERAL";
        case Page::Display: return "DISPLAY";
        case Page::Audio: return "AUDIO";
        case Page::Controls: return "CONTROLS";
        case Page::Camera: return vr_is_active() ? "CAMERA AND VR" : "CAMERA";
        case Page::Hud: return "HUD";
        case Page::Effects: return "EFFECTS";
        case Page::Rules: return "RACE RULES";
        case Page::Cheats: return "EXTRAS";
        case Page::Network: return "LAN RACE";
        case Page::Address: return "HOST ADDRESS";
        case Page::Saves: return "SAVE DATA";
        case Page::Bindings: return "CONTROLLER BINDINGS";
    }
    return "OPTIONS";
}

void Save() { Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame(); }
void ChangedPage() { ++generation; menu_pointer_cancel(); notice.clear(); reset_cycle_flash_menu(); }
void Push(Page page) { stack.push_back({page, {}}); ChangedPage(); }
void Open(bool paused, bool stock) {
    fromPause = paused; fromStockOptions = stock; legacy = false; legacyRequest = 0;
    liveNetwork = false; heldDirection = holdFrames = pageScroll = 0;
    stack.clear(); Push(paused ? Page::Pause : Page::Root);
}
void Close() { stack.clear(); liveNetwork = false; ChangedPage(); Save(); }
void Resume() {
    Close();
    if (fromPause && gIsGamePaused) {
        gIsGamePaused = 0;
        func_8028DF38();
        func_800C9F90(0);
    }
}
void BackPage() {
    play_sound2(SOUND_MENU_GO_BACK);
    if (stack.size() > 1) { stack.pop_back(); ChangedPage(); Save(); return; }
    if (fromPause) { Resume(); return; }
    const bool leaveOptions = fromStockOptions;
    Close();
    if (leaveOptions) func_8009E280();
}
void Finish() {
    if (fromPause) { Resume(); return; }
    const bool leaveOptions = fromStockOptions;
    Close();
    if (leaveOptions) func_8009E280();
}
int PointerContext() { return 240000 + (generation % 100000) * 8 + (gIsGamePaused & 7); }
bool Adjustable(const Row& row) {
    if (row.widget) return row.widget->type != WIDGET_BUTTON && row.widget->type != WIDGET_WINDOW_BUTTON;
    return row.action == Action::SoundMode || row.action == Action::Character || row.action == Action::Port ||
           (row.action >= Action::Octet0 && row.action <= Action::Octet3);
}

bool Prepare(WidgetInfo& widget) {
    widget.ResetDisables();
    if (widget.preFunc) widget.preFunc(widget);
    widget.options->disabled |= !widget.activeDisables.empty();
    return !widget.isHidden;
}

bool Include(const std::string& section, const std::string& sidebar, const WidgetInfo& widget) {
    const std::string_view cvar = widget.cVar ? widget.cVar : "";
    if (section == "Developer" || sidebar == "Search" || widget.name.find("Debug") == 0 ||
        cvar.find("gSettings.Menu.") == 0 || cvar == CVAR_IMGUI_CONTROLLER_NAV ||
        cvar == CVAR_ENABLE_MULTI_VIEWPORTS || cvar == "gNet.Window") return false;
    if (sidebar == "VR" && !vr_is_active()) return false;
    if (sidebar == "Camera" && vr_is_active()) return false;
#ifdef __ANDROID__
    if (widget.name == "Toggle Fullscreen" || cvar.find("Logitech") != std::string_view::npos) return false;
#endif
    if (widget.type == WIDGET_WINDOW_BUTTON) return std::strcmp(widget.windowName, "Input Editor") == 0;
    return widget.type <= WIDGET_BUTTON || widget.type == WIDGET_AUDIO_BACKEND || widget.type == WIDGET_VIDEO_BACKEND;
}

void BuildRows() {
    rows.clear();
    if (stack.empty()) return;
    const auto page = stack.back().page;
    if (page == Page::Pause) {
        rows.push_back({"RESUME RACE", nullptr, {}, Action::Resume});
        rows.push_back({"OPTIONS", nullptr, Page::Root});
        rows.push_back({"RESTART RACE", nullptr, {}, Action::Restart, !net_active()});
        if (race_mods_has_setup()) rows.push_back({"RACE SETUP", nullptr, {}, Action::RaceSetup, !net_active()});
        rows.push_back({"COURSE SELECT", nullptr, {}, Action::Course, !net_active()});
        rows.push_back({"DRIVER SELECT", nullptr, {}, Action::Driver, !net_active()});
        rows.push_back({"QUIT TO MENU", nullptr, {}, Action::Quit});
        return;
    }
    if (page == Page::Root) {
        for (auto category : {Page::General, Page::Display, Page::Audio, Page::Controls, Page::Camera,
                              Page::Hud, Page::Effects, Page::Rules, Page::Cheats, Page::Network, Page::Saves})
            rows.push_back({PageTitle(category), nullptr, category});
        return;
    }
    if (page == Page::Saves) {
        const bool available = !fromPause && (gMenuSelection == MAIN_MENU || gMenuSelection == OPTIONS_MENU);
        rows.push_back({"COPY GHOST DATA", nullptr, {}, Action::CopyGhost, available});
        rows.push_back({"ERASE SAVE DATA", nullptr, {}, Action::EraseSave, available});
        return;
    }
    if (page == Page::Network) {
        NetMenuState state{}; net_menu_state(&state);
        rows.push_back({"CHARACTER", nullptr, {}, Action::Character, !state.active && !fromPause});
        rows.push_back({"HOST ADDRESS", nullptr, Page::Address, Action::None, !state.active && !fromPause});
        rows.push_back({"UDP PORT", nullptr, {}, Action::Port, !state.active && !fromPause});
        rows.push_back({"HOST LAN", nullptr, {}, Action::Host, !state.active && !fromPause});
        rows.push_back({"JOIN LAN", nullptr, {}, Action::Join, !state.active && !fromPause});
        rows.push_back({"START RACE", nullptr, {}, Action::StartLan, state.canStart != 0});
        rows.push_back({"LEAVE LAN", nullptr, {}, Action::LeaveLan, state.active != 0 || state.inRace != 0});
        return;
    }
    if (page == Page::Address) {
        for (int i = 0; i < 4; ++i)
            rows.push_back({"ADDRESS PART " + std::to_string(i + 1), nullptr, {}, static_cast<Action>(int(Action::Octet0) + i)});
        return;
    }
    if (page == Page::Audio) rows.push_back({"SOUND MODE", nullptr, {}, Action::SoundMode});
    if (GameUI::mPortMenu) GameUI::mPortMenu->VisitOptions([page](const std::string& section, const std::string& sidebar, WidgetInfo& widget) {
        if (!Include(section, sidebar, widget) || !Prepare(widget)) return;
        const std::string_view cvar = widget.cVar ? widget.cVar : "";
        auto category = GameUI::Options::Classify(section, sidebar, cvar);
        if (widget.name == "Toggle Fullscreen") category = Page::Display;
        if (category != page) return;
        auto name = widget.name;
        auto format = name.find(':');
        if (format != std::string::npos) name.resize(format);
        if (widget.type == WIDGET_WINDOW_BUTTON) name = "CONTROLLER BINDINGS";
        if (widget.type == WIDGET_VIDEO_BACKEND) name = "RENDERER";
        if (widget.type == WIDGET_AUDIO_BACKEND) name = "AUDIO DEVICE API";
        if (cvar == "gFlatViewMode" || cvar == "gVRViewMode") name = "VIEW MODE";
        if (cvar == "gInterpolationFPS") name = "FRAME RATE";
        Row row{FontText(name), &widget};
        row.enabled = !widget.options->disabled;
        if (widget.type == WIDGET_AUDIO_BACKEND)
            row.enabled &= Ship::Context::GetRawInstance()->GetAudio()->GetAvailableAudioBackends()->size() > 1;
        if (widget.type == WIDGET_VIDEO_BACKEND)
            row.enabled &= Ship::Context::GetRawInstance()->GetWindow()->GetAvailableWindowBackends()->size() > 1;
        if (net_active() && (page == Page::Rules || page == Page::Cheats)) row.enabled = false;
        if (vr_is_active() && widget.type == WIDGET_WINDOW_BUTTON) row.enabled = false;
        rows.push_back(std::move(row));
    });
}

void RefreshRows() {
    BuildRows();
    size_t hash = stack.empty() ? 0 : static_cast<size_t>(stack.back().page) + 1;
    for (const auto& row : rows) {
        hash ^= std::hash<std::string>{}(row.label) + reinterpret_cast<size_t>(row.widget) +
                (row.enabled ? 1 : 0) + (hash << 6) + (hash >> 2);
    }
    if (hash != layoutHash) {
        layoutHash = hash;
        ++generation;
        menu_pointer_cancel();
    }
}

std::vector<int> ComboKeys(const std::unordered_map<int32_t, const char*>& choices) {
    std::vector<int> result;
    for (const auto& [key, label] : choices) result.push_back(key);
    std::sort(result.begin(), result.end());
    return result;
}
template <typename T> T* Pointer(WidgetInfo& widget) {
    auto value = std::get_if<T*>(&widget.valuePointer);
    return value ? *value : nullptr;
}
int IntValue(WidgetInfo& widget) {
    if (widget.type == WIDGET_CHECKBOX) { auto p = Pointer<bool>(widget); return p ? *p : 0; }
    if (widget.type == WIDGET_SLIDER_INT || widget.type == WIDGET_COMBOBOX) {
        auto p = Pointer<int32_t>(widget); return p ? *p : 0;
    }
    if (widget.type == WIDGET_CVAR_CHECKBOX)
        return CVarGetInteger(widget.cVar, std::static_pointer_cast<UIWidgets::CheckboxOptions>(widget.options)->defaultValue);
    if (widget.type == WIDGET_CVAR_COMBOBOX)
        return CVarGetInteger(widget.cVar, std::static_pointer_cast<UIWidgets::ComboboxOptions>(widget.options)->defaultIndex);
    return CVarGetInteger(widget.cVar, std::static_pointer_cast<UIWidgets::IntSliderOptions>(widget.options)->defaultValue);
}
void SetInt(WidgetInfo& widget, int value) {
    if (widget.type == WIDGET_CHECKBOX) { if (auto p = Pointer<bool>(widget)) *p = value != 0; }
    else if (widget.type == WIDGET_SLIDER_INT || widget.type == WIDGET_COMBOBOX) {
        if (auto p = Pointer<int32_t>(widget)) *p = value;
    } else CVarSetInteger(widget.cVar, value);
}
float FloatValue(WidgetInfo& widget) {
    auto option = std::static_pointer_cast<UIWidgets::FloatSliderOptions>(widget.options);
    if (widget.type == WIDGET_SLIDER_FLOAT) { auto p = Pointer<float>(widget); return p ? *p : option->defaultValue; }
    return CVarGetFloat(widget.cVar, option->defaultValue);
}
std::string WidgetValue(WidgetInfo& widget) {
    char text[64];
    switch (widget.type) {
        case WIDGET_CHECKBOX: case WIDGET_CVAR_CHECKBOX: return IntValue(widget) ? "ON" : "OFF";
        case WIDGET_COMBOBOX: case WIDGET_CVAR_COMBOBOX: {
            if (widget.cVar && std::strcmp(widget.cVar, "gPhone.HudLayout") == 0)
                return IntValue(widget) ? "HUD INWARD" : "CONTROLS INWARD";
            auto option = std::static_pointer_cast<UIWidgets::ComboboxOptions>(widget.options);
            auto value = option->comboMap.find(IntValue(widget));
            return value != option->comboMap.end() ? FontText(value->second) : "CUSTOM";
        }
        case WIDGET_SLIDER_INT: case WIDGET_CVAR_SLIDER_INT:
            return std::to_string(IntValue(widget));
        case WIDGET_SLIDER_FLOAT: case WIDGET_CVAR_SLIDER_FLOAT: {
            auto option = std::static_pointer_cast<UIWidgets::FloatSliderOptions>(widget.options);
            float value = FloatValue(widget);
            std::snprintf(text, sizeof(text), option->isPercentage ? "%.0f PCT" : "%.2f", option->isPercentage ? value * 100 : value);
            return text;
        }
        case WIDGET_AUDIO_BACKEND: {
            auto audio = Ship::Context::GetRawInstance()->GetAudio();
            auto at = audioBackendsMap.find(audio->GetCurrentAudioBackend());
            if (at == audioBackendsMap.end()) return "SYSTEM";
            return at->first == Ship::AudioBackend::WASAPI ? "WINDOWS" : FontText(at->second);
        }
        case WIDGET_VIDEO_BACKEND: {
            int value = Ship::Context::GetRawInstance()->GetConfig()->GetInt("Window.Backend.Id",
                static_cast<int>(Ship::Context::GetRawInstance()->GetWindow()->GetWindowBackend()));
            auto at = windowBackendsMap.find(value);
            return at == windowBackendsMap.end() ? "SYSTEM" : FontText(at->second);
        }
        default: return "";
    }
}

void ChangeWidget(WidgetInfo& widget, int direction) {
    if (!Prepare(widget) || widget.options->disabled) return;
    switch (widget.type) {
        case WIDGET_CHECKBOX: case WIDGET_CVAR_CHECKBOX: SetInt(widget, !IntValue(widget)); break;
        case WIDGET_COMBOBOX: case WIDGET_CVAR_COMBOBOX: {
            auto option = std::static_pointer_cast<UIWidgets::ComboboxOptions>(widget.options);
            int value = GameUI::Options::CycleKey(ComboKeys(option->comboMap), IntValue(widget), direction);
            if (widget.cVar && std::strcmp(widget.cVar, "gVRViewMode") == 0 && value == 1 && vr_fp_switch_locked())
                value = GameUI::Options::CycleKey(ComboKeys(option->comboMap), value, direction);
            SetInt(widget, value); break;
        }
        case WIDGET_SLIDER_INT: case WIDGET_CVAR_SLIDER_INT: {
            auto option = std::static_pointer_cast<UIWidgets::IntSliderOptions>(widget.options);
            int value = static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(IntValue(widget)) +
                static_cast<int64_t>(direction ? direction : 1) * option->step, option->min, option->max));
            SetInt(widget, value); break;
        }
        case WIDGET_SLIDER_FLOAT: case WIDGET_CVAR_SLIDER_FLOAT: {
            auto option = std::static_pointer_cast<UIWidgets::FloatSliderOptions>(widget.options);
            float value = GameUI::Options::Adjust(FloatValue(widget), option->defaultValue, option->min, option->max, option->step, direction);
            if (widget.type == WIDGET_SLIDER_FLOAT) { if (auto p = Pointer<float>(widget)) *p = value; }
            else CVarSetFloat(widget.cVar, value);
            break;
        }
        case WIDGET_WINDOW_BUTTON: Push(Page::Bindings); return;
        case WIDGET_BUTTON: if (direction != 0) return; break;
        case WIDGET_AUDIO_BACKEND: {
            auto audio = Ship::Context::GetRawInstance()->GetAudio();
            std::vector<int> available;
            for (auto backend : *audio->GetAvailableAudioBackends()) available.push_back(static_cast<int>(backend));
            std::sort(available.begin(), available.end());
            if (available.size() <= 1) return;
            audio->SetCurrentAudioBackend(static_cast<Ship::AudioBackend>(GameUI::Options::CycleKey(available,
                static_cast<int>(audio->GetCurrentAudioBackend()), direction)));
            notice = "RESTART TO APPLY"; break;
        }
        case WIDGET_VIDEO_BACKEND: {
            auto window = Ship::Context::GetRawInstance()->GetWindow();
            auto config = Ship::Context::GetRawInstance()->GetConfig();
            auto available = *window->GetAvailableWindowBackends();
            std::sort(available.begin(), available.end());
            if (available.size() <= 1) return;
            int current = config->GetInt("Window.Backend.Id", static_cast<int>(window->GetWindowBackend()));
            int value = GameUI::Options::CycleKey(available, current, direction);
            config->SetInt("Window.Backend.Id", value);
            config->SetString("Window.Backend.Name", windowBackendsMap.at(value));
            config->Save(); GameUI::mPortMenu->UpdateWindowBackendObjects();
            notice = "RESTART TO APPLY"; break;
        }
        default: return;
    }
    if (widget.callback) widget.callback(widget);
    if (widget.cVar && std::strcmp(widget.cVar, CVAR_TEXTURE_FILTER) == 0) notice = "RESTART TO APPLY";
    Save();
}

std::string RowValue(const Row& row) {
    if (row.widget) return WidgetValue(*row.widget);
    if (row.action == Action::SoundMode) {
        static const char* modes[] = {"STEREO", "HEADPHONES", "SURROUND", "MONO"};
        return modes[gSoundMode & 3];
    }
    NetMenuState state{};
    if (row.destination == Page::Address || row.action >= Action::Character) net_menu_state(&state);
    if (row.destination == Page::Address) return state.address;
    if (row.action == Action::Character) {
        static const char* names[] = {"MARIO", "LUIGI", "YOSHI", "TOAD", "DK", "WARIO", "PEACH", "BOWSER"};
        return names[std::clamp(state.character, 0, 7)];
    }
    if (row.action == Action::Port) return std::to_string(state.port);
    if (row.action >= Action::Octet0 && row.action <= Action::Octet3) {
        int octets[4]{};
        std::sscanf(state.address, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]);
        return std::to_string(octets[int(row.action) - int(Action::Octet0)]);
    }
    return "";
}

int Activate(const Row& row, int direction) {
    if (!row.enabled) return 0;
    if (direction == 0 || Adjustable(row)) play_sound2(SOUND_MENU_SELECT);
    if (row.destination) { if (direction == 0) Push(*row.destination); return 0; }
    if (row.widget) { ChangeWidget(*row.widget, direction); return 0; }
    if (!Adjustable(row) && direction != 0) return 0;
    switch (row.action) {
        case Action::Resume: Resume(); break;
        case Action::Restart: Close(); return 2;
        case Action::Quit:
            Close();
            if (net_active()) { net_menu_command(4); Resume(); return 0; }
            return 3;
        case Action::Course: Close(); return 4;
        case Action::Driver: Close(); return 5;
        case Action::RaceSetup: Close(); return 6;
        case Action::SoundMode:
            gSoundMode = (gSoundMode + (direction < 0 ? 3 : 1)) & 3;
            set_sound_mode();
            gSaveData.main.saveInfo.soundMode = gSoundMode;
            write_save_data_grand_prix_points_and_sound_mode(); update_save_data_backup();
            break;
        case Action::CopyGhost: case Action::EraseSave:
            if (gMenuSelection != OPTIONS_MENU) {
                gMenuSelection = OPTIONS_MENU;
                gFadeModeSelection = FADE_MODE_MAIN;
                fromStockOptions = true;
            }
            legacyRequest = row.action == Action::CopyGhost ? 1 : 2;
            legacy = true; menu_pointer_cancel(); break;
        case Action::Host: net_menu_command(1); break;
        case Action::Join: net_menu_command(2); break;
        case Action::StartLan: Close(); net_menu_command(3); break;
        case Action::LeaveLan: net_menu_command(4); break;
        case Action::Character: net_menu_adjust(0, direction < 0 ? -1 : 1); Save(); break;
        case Action::Port: net_menu_adjust(1, direction ? direction : 1); Save(); break;
        case Action::Octet0: case Action::Octet1: case Action::Octet2: case Action::Octet3:
            net_menu_adjust(2 + int(row.action) - int(Action::Octet0), direction ? direction : 1); Save(); break;
        default: break;
    }
    return 0;
}

int Input(struct Controller* controller) {
    if (stack.empty() || legacy) return 0;
    RefreshRows();
    stack.back().cursor.Clamp(static_cast<int>(rows.size()));
    u16 pressed = kart_input_get_menu_pressed(controller);
    const int directionHeld = (controller->button | controller->stickDirection) & (L_JPAD | R_JPAD);
    holdFrames = directionHeld && directionHeld == heldDirection ? holdFrames + 1 : 0;
    heldDirection = directionHeld;
    bool repeatable = !rows.empty() && Adjustable(rows[stack.back().cursor.selected]);
    if (repeatable && rows[stack.back().cursor.selected].widget) {
        auto type = rows[stack.back().cursor.selected].widget->type;
        repeatable = type != WIDGET_CHECKBOX && type != WIDGET_CVAR_CHECKBOX;
    }
    if (repeatable && holdFrames > 12 && holdFrames % 3 == 0) pressed |= directionHeld;
    if (backRequested) { backRequested = false; pressed = B_BUTTON; }
    if (is_screen_being_faded()) return 1;
    if (stack.back().page == Page::Bindings) {
        if (!bindingsPopup) {
            if (pressed & START_BUTTON) Finish();
            else if ((pressed & B_BUTTON) || menu_pointer_take(PointerContext()) == Back) BackPage();
        }
        return 1;
    }
    int hit = menu_pointer_take(PointerContext());
    if (hit == Back) pressed = B_BUTTON;
    else if (hit == Done) pressed = START_BUTTON;
    else if (hit == Previous || hit == Next) pageScroll += hit == Previous ? -1 : 1;
    else if (hit >= 0 && hit < static_cast<int>(rows.size()) * 3) {
        pageScroll = 0;
        stack.back().cursor.selected = hit / 3;
        pressed = hit % 3 == 1 ? L_JPAD : hit % 3 == 2 ? R_JPAD : A_BUTTON;
    }
    if (pressed & B_BUTTON) { BackPage(); return 1; }
    if (pressed & START_BUTTON) {
        Finish();
        return 1;
    }
    if (pageScroll) {
        stack.back().cursor.Scroll(pageScroll, static_cast<int>(rows.size()));
        pageScroll = 0; ChangedPage();
        play_sound2(SOUND_MENU_CURSOR_MOVE);
    }
    if (pressed & U_JPAD) {
        stack.back().cursor.Move(-1, static_cast<int>(rows.size())); ChangedPage();
        holdFrames = heldDirection = 0; play_sound2(SOUND_MENU_CURSOR_MOVE);
    }
    if (pressed & D_JPAD) {
        stack.back().cursor.Move(1, static_cast<int>(rows.size())); ChangedPage();
        holdFrames = heldDirection = 0; play_sound2(SOUND_MENU_CURSOR_MOVE);
    }
    if (!rows.empty()) {
        Row row = rows[stack.back().cursor.selected];
        int action = 0;
        if (pressed & L_JPAD) action = Activate(row, holdFrames > 60 ? -10 : -1);
        else if (pressed & R_JPAD) action = Activate(row, holdFrames > 60 ? 10 : 1);
        else if (pressed & A_BUTTON) action = Activate(row, 0);
        if (action) return action;
    }
    return 1;
}
} // namespace

extern "C" int game_options_is_open(void) { return !stack.empty() && !legacy; }
extern "C" int game_options_is_frontend(void) { return game_options_is_open() && !fromPause; }
extern "C" const char* game_options_page(void) {
    return legacy ? "SAVE DIALOG" : stack.empty() ? "" : PageTitle(stack.back().page);
}
extern "C" int game_options_context(void) { return game_options_is_open() ? PointerContext() : -1; }
extern "C" int game_options_row_count(void) { return game_options_is_open() ? static_cast<int>(rows.size()) : 0; }
extern "C" int game_options_row_info(int index, GameOptionRowInfo* info) {
    if (!info || !game_options_is_open() || index < 0 || index >= static_cast<int>(rows.size())) return 0;
    const auto& row = rows[index];
    *info = {};
    std::snprintf(info->label, sizeof(info->label), "%s", row.label.c_str());
    std::snprintf(info->value, sizeof(info->value), "%s", RowValue(row).c_str());
    info->index = index; info->enabled = row.enabled; info->adjustable = Adjustable(row);
    const int first = stack.back().cursor.First();
    info->visible = index >= first && index < first + Cursor::PageRows;
    return 1;
}
extern "C" void game_options_request(void) { requested = true; }
extern "C" int game_options_is_live_network(void) { return liveNetwork && game_options_is_open(); }
extern "C" void game_options_net_input(unsigned int buttons, int x, int y) {
    networkController.buttonPressed = buttons & ~networkController.button;
    networkController.button = buttons;
    const unsigned short stick = (x < -30 ? L_JPAD : x > 30 ? R_JPAD : 0) |
                                 (y < -30 ? D_JPAD : y > 30 ? U_JPAD : 0);
    networkController.stickPressed = stick & ~networkController.stickDirection;
    networkController.stickDirection = stick;
    if (!game_options_is_open() && (networkController.buttonPressed & START_BUTTON)) requested = true;
}
extern "C" void game_options_before_frame(void) {
    if (backRequested && !game_options_is_open()) {
        backRequested = false;
        gControllers[0].buttonPressed |= KART_MENU_CANCEL_BUTTON;
    }
    if (gGamestate == RACING && !fromPause && !stack.empty()) Close();
    if (gGamestate == RACING && net_active()) {
        if (requested && !game_options_is_open()) {
            Open(true, false); liveNetwork = true; requested = false;
            return;
        }
        if (liveNetwork && game_options_is_open()) {
            if (requested) { requested = false; backRequested = true; }
            Input(&networkController);
        }
        return;
    }
    if (requested && gGamestate == RACING && !gIsGamePaused) {
        gControllers[0].buttonPressed |= START_BUTTON;
    }
}
extern "C" void game_options_request_back(void) { backRequested = true; }
extern "C" void game_options_scroll(int pages) { pageScroll = std::clamp(pageScroll + pages, -1, 1); }

extern "C" int game_options_frontend_input(void) {
    if (gGamestate == RACING) return 0;
    if (fromPause && !stack.empty()) Close();
    if (legacy) {
        if (gMenuSelection != OPTIONS_MENU) { legacy = false; stack.clear(); }
        else if (!legacyRequest && gSubMenuSelection >= SUB_MENU_OPTION_MIN && gSubMenuSelection <= SUB_MENU_OPTION_MAX) {
            legacy = false; ChangedPage(); return 1;
        } else return 0;
    }
    const bool entry = menu_pointer_take(239999) == 0;
    if (requested || entry || (stack.empty() && gMenuSelection == OPTIONS_MENU && !is_screen_being_faded())) {
        requested = false;
        if (is_screen_being_faded()) return 0;
        if (stack.empty()) { Open(false, gMenuSelection == OPTIONS_MENU); return 1; }
        BackPage(); return 1;
    }
    if (!game_options_is_frontend()) return 0;
    return Input(&gControllers[0]);
}

extern "C" int game_options_pause_input(void) {
    if (!gIsGamePaused) { if (fromPause && !stack.empty()) Close(); return 0; }
    if (stack.empty() || !fromPause) { Open(true, false); requested = false; return 1; }
    if (requested) { requested = false; backRequested = true; }
    return Input(&gControllers[std::clamp<int>(gIsGamePaused - 1, 0, 3)]);
}

extern "C" int game_options_legacy_input(unsigned short* pressed) {
    if (!legacy || !legacyRequest) return 0;
    gSubMenuSelection = legacyRequest == 1 ? SUB_MENU_OPTION_COPY_CONTROLLER_PAK : SUB_MENU_OPTION_ERASE_ALL_DATA;
    *pressed = A_BUTTON;
    legacyRequest = 0;
    return 1;
}

extern "C" void game_options_draw_entry(void) {
    if (game_options_is_open() || is_screen_being_faded() || gGamestate == RACING ||
        gMenuSelection == MAIN_MENU || gMenuSelection == MAIN_MENU_FROM_QUIT) return;
    race_mods_draw_native_button(225, 212, 88, 27, "OPTIONS", 0);
    menu_pointer_target(239999, 0, 225, 212, 88, 27);
}

extern "C" void game_options_draw(void) {
    if (!game_options_is_open()) return;
    RefreshRows();
    auto& frame = stack.back();
    frame.cursor.Clamp(static_cast<int>(rows.size()));
    menu_pointer_clear_targets();
    const int alpha = vr_is_active() ? static_cast<int>(std::clamp(CVarGetFloat("gVRMenuOpacity", .5f), .3f, 1.f) * 255)
                                    : fromPause ? 160 : 100;
    race_mods_draw_native_backdrop(fromPause, alpha);
    set_text_color(TEXT_YELLOW);
    race_mods_text_fit_center(160, 29, PageTitle(frame.page), .9f, 275);
    if (frame.page == Page::Bindings) {
        race_mods_draw_native_button(18, 211, 90, 27, "BACK", 0);
        menu_pointer_target(PointerContext(), Back, 18, 211, 90, 27);
        return;
    }
    int first = frame.cursor.First(), end = std::min(first + Cursor::PageRows, static_cast<int>(rows.size()));
    for (int i = first; i < end; ++i) {
        const auto& row = rows[i];
        int y = 59 + (i - first) * 25;
        bool selected = i == frame.cursor.selected;
        if (selected) gDisplayListHead = draw_flash_select_case_slow(gDisplayListHead, 20, y - 15, 300, y + 9);
        else gDisplayListHead = draw_box_fill(gDisplayListHead, 20, y - 15, 300, y + 9, 0, 0, 0, 255);
        if (!row.enabled) set_text_color(TEXT_RED);
        else set_text_color_rainbow_if_selected(selected, 1, TEXT_YELLOW);
        const auto value = RowValue(row);
        race_mods_text_fit(26, y, row.label.c_str(), .62f, value.empty() ? 250.f : 156.f);
        if (!value.empty()) race_mods_text_fit_center(242, y, value.c_str(), .56f, 84);
        if (row.enabled) {
            menu_pointer_target(PointerContext(), i * 3, 20, y - 15, 280, 25);
            if (Adjustable(row)) {
                race_mods_draw_pointer_arrow(194, y - 4, -1);
                race_mods_draw_pointer_arrow(289, y - 4, 1);
                menu_pointer_target(PointerContext(), i * 3 + 1, 180, y - 15, 28, 25);
                menu_pointer_target(PointerContext(), i * 3 + 2, 276, y - 15, 24, 25);
            } else if (row.destination || row.widget) race_mods_draw_pointer_arrow(289, y - 4, 1);
        }
    }
    set_text_color(TEXT_YELLOW);
    std::string status = notice;
    if (status.empty() && frame.page == Page::Network) {
        NetMenuState state{}; net_menu_state(&state);
        status = FontText(state.status);
    }
    if (status.empty() && frame.page == Page::Saves && !rows.empty() && !rows[0].enabled)
        status = "AVAILABLE FROM THE MAIN MENU";
    if (status.empty() && rows.size() > Cursor::PageRows)
        status = std::to_string(first / Cursor::PageRows + 1) + " / " + std::to_string((rows.size() + Cursor::PageRows - 1) / Cursor::PageRows);
    if (!status.empty()) race_mods_text_fit_center(160, 207, status.c_str(), .45f, 276);
    race_mods_draw_native_button(18, 212, 83, 26, "BACK", 0);
    race_mods_draw_native_button(207, 212, 95, 26, fromPause ? "RESUME" : "DONE", 0);
    menu_pointer_target(PointerContext(), Back, 18, 212, 83, 26);
    menu_pointer_target(PointerContext(), Done, 207, 212, 95, 26);
    if (first > 0) {
        race_mods_draw_pointer_arrow(127, 225, -1);
        menu_pointer_target(PointerContext(), Previous, 109, 212, 37, 26);
    }
    if (end < static_cast<int>(rows.size())) {
        race_mods_draw_pointer_arrow(175, 225, 1);
        menu_pointer_target(PointerContext(), Next, 155, 212, 37, 26);
    }
}

namespace GameUI {
bool OptionsBindingsOpen() { return game_options_is_open() && stack.back().page == Page::Bindings; }
void DrawOptionsBindings(float x, float y, float width, float height) {
    static bool wasOpen = false;
    if (!OptionsBindingsOpen()) { wasOpen = false; return; }
    const bool entering = !wasOpen;
    wasOpen = true;
    auto editor = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Input Editor");
    if (!editor) return;
    const float scale = height / 240.f;
    const float center = x + width * .5f;
    ImGui::SetCursorScreenPos(ImVec2(center - 138.f * scale, y + 44.f * scale));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 240));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 240, 96, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(24, 24, 24, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(72, 72, 72, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(100, 100, 100, 255));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 32, 32, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 80, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(112, 112, 112, 255));
    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(40, 40, 40, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(80, 80, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(112, 112, 112, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
    if (ImGui::BeginChild("GameControllerBindings", ImVec2(276.f * scale, 161.f * scale), false)) {
        if (entering) ImGui::SetWindowFocus();
        ImGui::SetWindowFontScale(std::clamp(scale * .65f, 1.f, 2.f));
        editor->DrawElement();
        bindingsPopup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(11);
}
} // namespace GameUI
