#include "GameOptionsModel.h"
#include "game_options.h"
#include "port/net/NetRace.h"
#include "port/net/Protocol.h"
#include <libultraship.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "main.h"
#include "menus.h"
#include "menu_items.h"
#include "menu_pointer.h"
#include "menu_footer_model.h"
#include "quit_menu.h"
#include "race_mods_menu.h"
#include "kart_input.h"
#include "kart_transmission.h"
#include "audio/external.h"
#include <sounds.h>
void reset_cycle_flash_menu(void);
}

namespace {
using GameUI::Lobby::Wrap;
using GameUI::Options::FontText;
enum class Page { Main, Online, LocalSetup, HostSetup, Browser, Direct, Lobby, Standings, Party, Editor };
enum Action { Setting, OpenHost, OpenBrowser, OpenDirect, Refresh, JoinServer, Text, Port,
              OpenParty, Host, Join, StartLocal, Ready, Leave, SessionBack, Character, Transmission };
struct Row { std::string label, value; Action action; int argument = 0; bool enabled = true; std::string detail; };
constexpr bool HasRows(Page destination) {
    return destination == Page::Online || destination == Page::LocalSetup || destination == Page::HostSetup ||
           destination == Page::Browser || destination == Page::Direct || destination == Page::Party;
}
constexpr bool Adjustable(Action action) {
    return action == Setting || action == Port || action == Character || action == Transmission;
}
constexpr bool CapabilityAvailable(bool local, unsigned int capabilities, unsigned int feature) {
    return local || (capabilities & feature) != 0;
}
static_assert(CapabilityAvailable(true, 0, NET_CAP_ONLINE_ITEMS));
static_assert(!CapabilityAvailable(false, 0, NET_CAP_ONLINE_ITEMS));
static_assert(CapabilityAvailable(false, NET_CAP_ONLINE_ITEMS, NET_CAP_ONLINE_ITEMS));
static_assert(!CapabilityAvailable(false, NET_CAP_ONLINE_BATTLE, NET_CAP_ONLINE_ITEMS));
static_assert(CapabilityAvailable(false, NET_CAP_ONLINE_BATTLE, NET_CAP_ONLINE_BATTLE));
static_assert(!HasRows(Page::Main) && !HasRows(Page::Editor) && !HasRows(Page::Lobby) && !HasRows(Page::Standings));
static_assert(HasRows(Page::Online) && HasRows(Page::LocalSetup) && HasRows(Page::HostSetup) &&
              HasRows(Page::Browser) && HasRows(Page::Direct) && HasRows(Page::Party));
static_assert(Adjustable(Setting) && Adjustable(Port) && Adjustable(Character) && Adjustable(Transmission) &&
              !Adjustable(Text) && !Adjustable(Ready) && !Adjustable(JoinServer));
Page page = Page::Main, editorReturn = Page::Main, partyReturn = Page::Main;
Page rowsPage = Page::Main;
std::vector<Row> rows;
int selected = 0, mainSelected = 0, generation = 0, partyController = 0, editorField = 0, keyboard = 0;
bool backRequested = false, lowerCase = false, sawSession = false, singlePlayer = false;
std::string draft, editorNotice, rowLayout;
Controller networkPad{};
constexpr int Back = 1000, Next = 1001, Previous = 1002, Primary = 1003;
constexpr int KeyDelete = 1100, KeyAccept = 1101, KeyCase = 1102, KeySpace = 1103;
enum class SessionAction { None, Start, Ready, Return };
constexpr SessionAction ResolveSessionAction(int hit, bool startPressed, bool host, bool canStart, bool finished, bool inLobby) {
    if (hit == Primary || startPressed) {
        if (host && canStart && (inLobby || finished)) return SessionAction::Start;
        if (!host && !finished && inLobby) return SessionAction::Ready;
    } else if (hit == 1004) {
        if (finished && host) return SessionAction::Return;
        if (!finished && inLobby) return SessionAction::Ready;
    }
    return SessionAction::None;
}
static_assert(ResolveSessionAction(Primary, true, true, false, false, true) == SessionAction::None);
static_assert(ResolveSessionAction(Primary, false, true, true, false, true) == SessionAction::Start);
static_assert(ResolveSessionAction(1004, false, true, false, false, true) == SessionAction::Ready);
static_assert(ResolveSessionAction(1004, false, false, false, false, false) == SessionAction::None);
static_assert(ResolveSessionAction(Primary, true, true, true, false, false) == SessionAction::None);
static_assert(ResolveSessionAction(1004, false, true, false, true, false) == SessionAction::Return);
constexpr char Keys[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-:/";
constexpr const char* Characters[] = {"MARIO", "LUIGI", "YOSHI", "TOAD", "D.K.", "WARIO", "PEACH", "BOWSER"};
constexpr const char* Cups[] = {"MUSHROOM", "FLOWER", "STAR", "SPECIAL"};
constexpr const char* Modes[] = {"GRAND PRIX", "VERSUS", "BATTLE"};
constexpr const char* EngineClasses[] = {"50 CC", "100 CC", "150 CC", "EXTRA"};
constexpr const char* ItemModes[] = {"NORMAL", "RANDOM", "NONE"};
constexpr const char* Courses[] = {
    "LUIGI RACEWAY", "MOO MOO FARM", "KOOPA TROOPA BEACH", "KALIMARI DESERT",
    "TOADS TURNPIKE", "FRAPPE SNOWLAND", "CHOCO MOUNTAIN", "MARIO RACEWAY",
    "WARIO STADIUM", "SHERBET LAND", "ROYAL RACEWAY", "BOWSERS CASTLE",
    "D.K. JUNGLE PARKWAY", "YOSHI VALLEY", "BANSHEE BOARDWALK", "RAINBOW ROAD",
    "BLOCK FORT", "SKYSCRAPER", "DOUBLE DECK", "BIG DONUT"};

int Context() { return 700000 + (generation % 10000) * 16 + int(page); }
bool MainMenu() {
    return gGamestate != RACING && (gMenuSelection == MAIN_MENU || gMenuSelection == MAIN_MENU_FROM_QUIT) &&
           gMainMenuSelection != MAIN_MENU_DATA && gMainMenuSelection != MAIN_MENU_OPTION;
}
bool Phase(const NetMenuState& state, KartNet::Phase phase) { return state.phase == int(phase); }
bool SessionPage(const NetMenuState& state) {
    return (state.active || state.inRace) && (Phase(state, KartNet::Phase::Lobby) || Phase(state, KartNet::Phase::Finished) ||
                           Phase(state, KartNet::Phase::Connecting) || Phase(state, KartNet::Phase::Loading) ||
                           Phase(state, KartNet::Phase::Error) || Phase(state, KartNet::Phase::Disconnected));
}
bool Visible() {
    if (game_options_is_open()) return false;
    NetMenuState state{}; net_menu_state(&state);
    return MainMenu() || (gGamestate == RACING && SessionPage(state));
}
void Change(Page destination) {
    page = destination; selected = destination == Page::Lobby || destination == Page::Standings ? 5 : 0; ++generation;
    menu_pointer_cancel(); reset_cycle_flash_menu();
}
void Sound(bool back = false) { play_sound2(back ? SOUND_MENU_GO_BACK : SOUND_MENU_SELECT); }
bool Joined(int controller) { NetPartyInfo info{}; return net_party_info(controller, &info) && info.joined; }
int PartyCount() { int count = 0; for (int i = 0; i < 4; ++i) count += Joined(i) ? 1 : 0; return count; }
void EnsureParty() { if (PartyCount() == 0) net_party_join(0); }
const char* Course(int course) { return course >= 0 && course < 20 ? Courses[course] : "UNKNOWN COURSE"; }
std::string Number(int number) { return std::to_string(number); }
std::string Mask(bool set) { return set ? "SET" : "NONE"; }
template <typename T> std::string PlayerName(const T& player) {
    return Characters[std::clamp(player.character, 0, 7)];
}
void Button(int id, int x, int y, int w, int h, const char* label, bool focus, bool enabled = true) {
    race_mods_draw_native_button(x, y, w, h, label, focus && enabled);
    if (enabled && !is_screen_being_faded()) menu_pointer_target(Context(), id, x, y, w, h);
}
void Title(const char* title) {
    set_text_color(TEXT_YELLOW); race_mods_text_fit_center(160, 26, title, .86f, 284);
}
void Status(const NetMenuState& state) {
    set_text_color(TEXT_YELLOW);
    race_mods_text_fit_center(160, 199, FontText(state.status).c_str(), .43f, 282);
}
void SettingRow(const char* label, int field, std::string value, bool enabled = true) {
    rows.push_back({label, std::move(value), Setting, field, enabled});
}
void SetupRows(const NetMenuState& state, bool local) {
    const unsigned int capabilities = net_capabilities();
    const bool itemsAvailable = CapabilityAvailable(local, capabilities, NET_CAP_ONLINE_ITEMS);
    const bool battleAvailable = CapabilityAvailable(local, capabilities, NET_CAP_ONLINE_BATTLE);
    SettingRow("RACE TYPE", NET_SETTING_MODE, Modes[std::clamp(state.mode, 0, 2)]);
    if (state.mode == 0) SettingRow("CUP", NET_SETTING_CUP, Cups[std::clamp(state.cup, 0, 3)]);
    else SettingRow(state.mode == 2 ? "ARENA" : "COURSE", NET_SETTING_COURSE, Course(state.course));
    SettingRow("ENGINE CLASS", NET_SETTING_CC, EngineClasses[std::clamp(state.cc, 0, 3)]);
    if (state.mode == 2) SettingRow("BALLOONS", NET_SETTING_BALLOONS, Number(state.balloons));
    else SettingRow("LAPS", NET_SETTING_LAPS, Number(state.laps));
    SettingRow("PLAYER CAPACITY", NET_SETTING_MAX_PLAYERS, Number(state.maxPlayers));
    SettingRow("CPU FILL", NET_SETTING_CPU_FILL, state.cpuFill ? "ON" : "OFF", state.mode != 2 || state.cpuFill);
    SettingRow("MIRROR", NET_SETTING_MIRROR, state.mirror ? "ON" : "OFF", state.mode != 2);
    SettingRow("ITEMS", NET_SETTING_ITEMS, itemsAvailable ? ItemModes[std::clamp(state.items, 0, 2)] : "NETWORK OFF", itemsAvailable);
    if (!local) {
        rows.push_back({"SERVER NAME", FontText(state.name), Text, 1, !state.active});
        SettingRow("CONNECTION", NET_SETTING_SCOPE, state.scope == 2 ? "DIRECT INTERNET" : "LAN", !state.active);
        rows.push_back({"SERVER PASSWORD", Mask(state.passwordSet), Text, 2, !state.active});
        rows.push_back({"UDP PORT", Number(state.port), Text, 4, !state.active});
    }
    rows.push_back({state.active ? "RETURN TO LOBBY" : local ? "CONTINUE TO LOBBY" : "HOST GAME", "",
                    state.active ? SessionBack : local ? StartLocal : Host});
    if (local && state.mode == 2 && PartyCount() < 2) {
        rows.back().enabled = false; rows.back().detail = "2 TO 4 LOCAL DRIVERS";
    }
    if (!battleAvailable && state.mode == 2) rows.front().detail = "ONLINE BATTLE PENDING";
}
void Rows(const NetMenuState& state) {
    rows.clear();
    switch (page) {
        case Page::Online:
            rows = {{"HOST GAME", "", OpenHost}, {"LAN SERVER BROWSER", "", OpenBrowser},
                    {"DIRECT CONNECT", "", OpenDirect}};
            break;
        case Page::LocalSetup: SetupRows(state, true); break;
        case Page::HostSetup: SetupRows(state, false); break;
        case Page::Direct:
            rows = {{"HOST ADDRESS", FontText(state.address), Text, 0}, {"UDP PORT", Number(state.port), Text, 4},
                    {"PASSWORD", Mask(state.passwordSet), Text, 3}, {"JOIN GAME", "", Join}};
            break;
        case Page::Browser:
            rows.push_back({"REFRESH LAN", "", Refresh});
            for (int i = 0; i < net_browser_count(); ++i) {
                NetServerInfo server{};
                if (!net_browser_info(i, &server)) continue;
                bool joinable = GameUI::Lobby::BrowserJoinable(PartyCount(), server.players, server.maxPlayers,
                                                               server.compatible != 0, server.phase);
                std::string seats = Number(server.players) + "/" + Number(server.maxPlayers);
                std::string detail = std::string(Modes[std::clamp(server.mode, 0, 2)]) + " / " +
                    (server.mode == 0 ? Cups[std::clamp(server.cup, 0, 3)] : Course(server.course));
                if (server.password) detail += " / LOCKED";
                if (!server.compatible) detail += " / VERSION";
                else if (!joinable) detail += " / FULL OR IN RACE";
                else detail += " / " + Number(server.ping) + " MS";
                rows.push_back({FontText(server.name), seats, JoinServer, i, joinable, detail});
            }
            break;
        case Page::Party:
            if (!Joined(partyController)) {
                rows.push_back({"JOIN DRIVER " + Number(partyController + 1), "", OpenParty, partyController});
            } else {
                NetPartyInfo player{}; net_party_info(partyController, &player);
                const bool editable = !state.active || Phase(state, KartNet::Phase::Lobby);
                rows.push_back({"CHARACTER", PlayerName(player), Character, partyController, editable});
                rows.push_back({"TRANSMISSION", player.transmission == KART_TRANSMISSION_MANUAL ? "MANUAL" : "AUTO",
                                Transmission, partyController, editable});
                rows.push_back({"READY", player.ready ? "YES" : "NO", Ready, partyController});
                rows.push_back({"LEAVE LOCAL PARTY", "", Leave, partyController, editable && partyController != 0});
            }
            break;
        default: break;
    }
    std::string layout = Number(int(page)) + ";";
    for (const auto& row : rows) layout += row.label + ":" + Number(row.argument) + ":" + (row.enabled ? "Y;" : "N;");
    if (layout != rowLayout) { rowLayout = layout; ++generation; menu_pointer_cancel(); }
    selected = std::clamp(selected, 0, std::max(0, int(rows.size()) - 1));
    rowsPage = page;
}
void Edit(int field, const NetMenuState& state) {
    editorField = field; editorReturn = page; keyboard = field == 4 ? 26 : 0; lowerCase = false; editorNotice.clear();
    if (field == 4) draft = Number(state.port);
    else if (field <= 1) draft = field == 0 ? state.address : state.name;
    else {
        char current[64]{}; net_menu_get_text(NET_TEXT_PASSWORD, current, sizeof(current)); draft = current;
    }
    Change(Page::Editor);
}
void BackPage(const NetMenuState& state) {
    Sound(true);
    if (page == Page::Editor) { Change(editorReturn); return; }
    if (page == Page::Party) { Change(partyReturn); return; }
    if (state.active && (page == Page::HostSetup || page == Page::LocalSetup)) { Change(Page::Lobby); return; }
    if (page == Page::Lobby || page == Page::Standings) {
        net_menu_command(4); sawSession = false; Change(Page::Main); return;
    }
    if (page == Page::Main || page == Page::Online || page == Page::LocalSetup) {
        if (page != Page::Main) { Change(Page::Main); return; }
        func_8009E0F0(20); func_800CA330(0x19); gMenuFadeType = MENU_FADE_TYPE_BACK;
        return;
    }
    Change(Page::Online);
}
void Activate(const Row& row, int direction, const NetMenuState& state) {
    if (!row.enabled) return;
    const int step = direction ? direction : 1;
    switch (row.action) {
        case Setting:
            net_menu_adjust(row.argument, row.argument == NET_SETTING_MODE ?
                GameUI::Lobby::SetupModeDirection(state.mode, step, singlePlayer && page == Page::LocalSetup) : step);
            if (row.argument == NET_SETTING_SCOPE) {
                NetMenuState updated{}; net_menu_state(&updated);
                if (updated.scope == 0) net_menu_adjust(row.argument, step);
            }
            if (row.argument == NET_SETTING_COURSE || row.argument == NET_SETTING_MODE) {
                NetMenuState updated{}; net_menu_state(&updated);
                for (int i = 0; i < 20 && ((updated.mode == 2 && updated.course < 16) ||
                     (updated.mode != 2 && updated.course >= 16)); ++i) {
                    net_menu_adjust(NET_SETTING_COURSE, step); net_menu_state(&updated);
                }
                if (updated.mode == 2 && updated.cpuFill) net_menu_adjust(NET_SETTING_CPU_FILL, 1);
            }
            if (row.argument == NET_SETTING_BALLOONS) {
                NetMenuState updated{}; net_menu_state(&updated);
                for (int i = 0; i < 5 && updated.balloons > 3; ++i) {
                    net_menu_adjust(NET_SETTING_BALLOONS, step); net_menu_state(&updated);
                }
            }
            break;
        case Port: net_menu_adjust(1, step); break;
        case Character: net_party_adjust(row.argument, 0, step); break;
        case Transmission: net_party_adjust(row.argument, 1, step); break;
        default:
            if (direction) return;
            switch (row.action) {
                case OpenHost:
                    EnsureParty();
                    if (state.scope == 0) net_menu_adjust(NET_SETTING_SCOPE, 1);
                    Change(Page::HostSetup); break;
                case OpenBrowser: EnsureParty(); net_menu_command(NET_REFRESH); Change(Page::Browser); break;
                case OpenDirect: EnsureParty(); Change(Page::Direct); break;
                case Refresh: net_menu_command(NET_REFRESH); break;
                case JoinServer: {
                    NetServerInfo server{};
                    if (!net_browser_info(row.argument, &server)) break;
                    if (server.password) {
                        net_menu_text(NET_TEXT_ADDRESS, server.address);
                        net_menu_text(NET_TEXT_PORT, Number(server.port).c_str());
                        Change(Page::Direct); Edit(3, state);
                    } else {
                        net_menu_text(NET_TEXT_PASSWORD, "");
                        net_browser_join(row.argument); sawSession = true; Change(Page::Lobby);
                    }
                    break;
                }
                case Text: Edit(row.argument, state); break;
                case OpenParty:
                    if (page == Page::Party) { net_party_join(row.argument); break; }
                    partyReturn = page; partyController = 0; Change(Page::Party); break;
                case Host: net_menu_command(1); sawSession = true; Change(Page::Lobby); break;
                case Join: net_menu_command(2); sawSession = true; Change(Page::Lobby); break;
                case StartLocal: net_menu_command(5); sawSession = true; Change(Page::Lobby); break;
                case SessionBack: Change(Page::Lobby); break;
                case Ready: {
                    NetPartyInfo player{};
                    net_party_info(row.argument, &player); net_party_ready(row.argument, !player.ready); break;
                }
                case Leave:
                    net_party_leave(row.argument); break;
                default: break;
            }
    }
    Sound();
}
void PartyInput(int controller, unsigned short buttons, bool inLobby) {
    using GameUI::Lobby::PartyAction;
    switch (GameUI::Lobby::ResolvePartyAction(Joined(controller), inLobby, buttons & START_BUTTON,
            buttons & B_BUTTON, buttons & A_BUTTON, buttons & (L_JPAD | R_JPAD), buttons & (U_JPAD | D_JPAD))) {
        case PartyAction::Join: net_party_join(controller); break;
        case PartyAction::Leave: net_party_leave(controller); break;
        case PartyAction::Ready: {
            NetPartyInfo player{}; net_party_info(controller, &player); net_party_ready(controller, !player.ready); break;
        }
        case PartyAction::Character: net_party_adjust(controller, 0, buttons & L_JPAD ? -1 : 1); break;
        case PartyAction::Transmission: net_party_adjust(controller, 1, buttons & U_JPAD ? -1 : 1); break;
        default: break;
    }
}
void EditorInput(unsigned short buttons, int hit) {
    auto move = editorField == 4 ? GameUI::Lobby::PortKeyboardMove : GameUI::Lobby::KeyboardMove;
    if (buttons & U_JPAD) keyboard = move(keyboard, 0, -1);
    if (buttons & D_JPAD) keyboard = move(keyboard, 0, 1);
    if (buttons & L_JPAD) keyboard = move(keyboard, -1, 0);
    if (buttons & R_JPAD) keyboard = move(keyboard, 1, 0);
    if (buttons & A_BUTTON) hit = keyboard < 40 ? keyboard : keyboard == 40 ? KeyDelete : keyboard == 41 ? KeyCase : keyboard == 42 ? KeySpace : KeyAccept;
    if (buttons & START_BUTTON) hit = KeyAccept;
    if (hit == KeyDelete) { if (!draft.empty()) draft.pop_back(); }
    else if (hit == KeyCase) lowerCase = !lowerCase;
    else if (hit == KeyAccept) {
        if (editorField == NET_TEXT_NAME && draft.empty()) { editorNotice = "NAME IS EMPTY"; return; }
        if (editorField == 4 && !GameUI::Lobby::ValidPort(draft)) { editorNotice = "PORT 1 TO 65535"; return; }
        net_menu_text(editorField == 4 ? NET_TEXT_PORT : editorField >= 2 ? NET_TEXT_PASSWORD : editorField, draft.c_str());
        Change(editorReturn); Sound();
    } else if (hit >= 0 && hit < 40 && draft.size() < (editorField == 0 ? 63u : 47u)) {
        char c = Keys[hit];
        if (editorField == 4 && (c < '0' || c > '9' || draft.size() >= 5)) return;
        if (lowerCase && c >= 'A' && c <= 'Z') c += 'a' - 'A';
        draft += c; keyboard = hit; Sound();
    } else if (hit == KeySpace && editorField > 0 && editorField < 4 && draft.size() < 47) draft += ' ';
}
void MainInput(unsigned short buttons, int hit) {
    if (quit_menu_input(buttons)) return;
    if ((hit >= 400 && hit < 403) || (hit >= 410 && hit < 414)) {
        unsigned short none = 0;
        main_menu_footer_input(&none, 0);
    }
    int footerHit = menu_pointer_take(menu_pointer_main_context());
    if (main_menu_footer_focused() || footerHit >= MENU_FOOTER_FIRST_ID ||
        (buttons & D_JPAD)) {
        if (main_menu_footer_input(&buttons, footerHit)) return;
    }
    if (buttons & B_BUTTON) { NetMenuState state{}; BackPage(state); return; }
    if (hit >= 410 && hit < 414) {
        const int controller = hit - 410;
        if (!Joined(controller)) net_party_join(controller);
        else if (controller) net_party_leave(controller);
        Sound(); return;
    }
    if (hit >= 400 && hit < 403) { mainSelected = hit - 400; buttons = A_BUTTON; }
    if (buttons & (L_JPAD | R_JPAD)) {
        mainSelected = Wrap(mainSelected + (buttons & L_JPAD ? -1 : 1), 3);
        reset_cycle_flash_menu(); play_sound2(SOUND_MENU_CURSOR_MOVE);
    }
    if (!(buttons & (A_BUTTON | START_BUTTON))) return;
    EnsureParty();
    singlePlayer = mainSelected == 0;
    net_party_set_limit(singlePlayer ? 1 : 4);
    if (mainSelected == 2) Change(Page::Online);
    else {
        if (singlePlayer) {
            NetMenuState state{}; net_menu_state(&state);
            if (state.mode == 2) net_menu_adjust(NET_SETTING_MODE, 1);
            if (!state.cpuFill) net_menu_adjust(NET_SETTING_CPU_FILL, 1);
        }
        Change(Page::LocalSetup);
    }
    Sound();
}
void SessionInput(unsigned short buttons, int hit, const NetMenuState& state) {
    const bool finished = page == Page::Standings;
    const bool inLobby = Phase(state, KartNet::Phase::Lobby);
    if (buttons & L_JPAD) selected = selected < 4 ? Wrap(selected - 1, 4) : 4 + Wrap(selected - 5, 4);
    if (buttons & R_JPAD) selected = selected < 4 ? Wrap(selected + 1, 4) : 4 + Wrap(selected - 3, 4);
    if (buttons & U_JPAD) selected = finished ? 5 : std::clamp(selected - 4, 0, 3);
    if (buttons & D_JPAD) selected = selected < 4 ? 5 : selected;
    if (buttons & A_BUTTON) hit = selected < 4 ? 400 + selected : selected == 4 ? Back : selected == 5 ? 1004 : selected == 6 ? 1006 : Primary;
    if (hit == Back) { BackPage(state); return; }
    if (hit >= 400 && hit < 404) {
        partyController = hit - 400;
        if (inLobby && !finished && Joined(partyController)) { partyReturn = page; Change(Page::Party); }
        return;
    }
    if (hit == 1006 && state.host && !finished && inLobby) {
        Change(state.localOnly ? Page::LocalSetup : Page::HostSetup); return;
    }
    switch (ResolveSessionAction(hit, (buttons & START_BUTTON) != 0, state.host != 0, state.canStart != 0, finished, inLobby)) {
        case SessionAction::Start: net_menu_command(NET_START); break;
        case SessionAction::Return: net_menu_command(NET_RETURN_LOBBY); break;
        case SessionAction::Ready: {
            NetPartyInfo driver{};
            if (net_party_info(0, &driver)) net_party_ready(0, !driver.ready);
            break;
        }
        default: break;
    }
}
void DrawMain() {
    gDisplayListHead = render_menu_textures(gDisplayListHead, seg2_game_select_texture, 60, 47);
    constexpr const char* labels[] = {"1P", "LOCAL", "ONLINE"};
    for (int mode = 0; mode < 3; ++mode) {
        const auto rect = GameUI::Lobby::MainModeRect(mode);
        const bool focused = mainSelected == mode && !main_menu_footer_focused();
        if (focused) gDisplayListHead = draw_flash_select_case_slow(gDisplayListHead, rect.x, rect.y, rect.x + rect.w - 1, rect.y + rect.h - 1);
        else gDisplayListHead = func_80098FC8(gDisplayListHead, rect.x, rect.y, rect.x + rect.w - 1, rect.y + rect.h - 1);
        if (mode == 0) {
            MenuTexture tile[] = {seg2_menu_1p_column[0], {}};
            gDisplayListHead = render_menu_textures(gDisplayListHead, tile, rect.x, rect.y);
        } else {
            set_text_color(TEXT_BLUE_GREEN_RED_CYCLE_1);
            race_mods_text_fit_center(rect.x + rect.w / 2, rect.y + 24, labels[mode], .72f, rect.w - 6);
            set_text_color(TEXT_YELLOW);
            race_mods_text_fit_center(rect.x + rect.w / 2, rect.y + 45, "GAME", .65f, rect.w - 6);
        }
        if (focused) gDisplayListHead = render_menu_textures(gDisplayListHead, D_800E8234[1], rect.x, rect.y);
        if (!is_screen_being_faded()) menu_pointer_target(Context(), 400 + mode, rect.x, rect.y, rect.w, rect.h);
    }
    constexpr int colors[][3] = {{240, 72, 64}, {48, 192, 96}, {64, 144, 248}, {248, 208, 48}};
    for (int i = 0; i < 4; ++i) {
        const auto rect = GameUI::Lobby::MainPlayerRect(i);
        const bool joined = Joined(i);
        gDisplayListHead = draw_box_fill(gDisplayListHead, rect.x, rect.y, rect.x + rect.w - 1, rect.y + rect.h - 1,
                                       colors[i][0], colors[i][1], colors[i][2], joined ? 255 : 100);
        gDisplayListHead = draw_box_fill(gDisplayListHead, rect.x + 2, rect.y + 2, rect.x + rect.w - 3, rect.y + rect.h - 3, 0, 0, 0, 255);
        set_text_color(TEXT_YELLOW);
        race_mods_text_fit_center(rect.x + 15, rect.y + 19, (Number(i + 1) + "P").c_str(), .55f, 24);
        race_mods_text_fit_center(rect.x + 49, rect.y + 19, joined ? "IN" : "JOIN", .48f, 38);
        if (!is_screen_being_faded()) menu_pointer_target(Context(), 410 + i, rect.x, rect.y, rect.w, rect.h);
    }
    main_menu_footer_draw();
}
void DrawEditor() {
    constexpr const char* titles[] = {"HOST ADDRESS", "SERVER NAME", "SERVER PASSWORD", "JOIN PASSWORD", "UDP PORT"};
    Title(titles[editorField]);
    gDisplayListHead = draw_box_fill(gDisplayListHead, 16, 39, 304, 74, 0, 0, 0, 255);
    set_text_color(TEXT_YELLOW);
    const std::string text = editorField == 2 || editorField == 3 ? GameUI::Lobby::HiddenText(draft.size()) : FontText(draft);
    race_mods_text_fit_center(160, 62, text.empty() ? "-" : text.c_str(), .65f, 274);
    if (editorField == 4) {
        for (int i = 0; i < 12; ++i) {
            int key = i < 9 ? 27 + i : i == 9 ? 40 : i == 10 ? 26 : 43;
            std::string label = i < 9 ? Number(i + 1) : i == 9 ? "DELETE" : i == 10 ? "0" : "DONE";
            int id = key == 40 ? KeyDelete : key == 43 ? KeyAccept : key;
            Button(id, 64 + i % 3 * 65, 82 + i / 3 * 29, 62, 27, label.c_str(), keyboard == key);
        }
    } else {
        for (int i = 0; i < 40; ++i) {
            char label[] = { Keys[i], 0 };
            Button(i, 16 + i % 10 * 29, 83 + i / 10 * 25, 27, 23, label, keyboard == i);
        }
        Button(KeyDelete, 16, 185, 70, 24, "DELETE", keyboard == 40);
        Button(KeyCase, 90, 185, 66, 24, lowerCase ? "LOWER" : "UPPER", keyboard == 41);
        Button(KeySpace, 160, 185, 66, 24, "SPACE", keyboard == 42, editorField > 0);
        Button(KeyAccept, 230, 185, 74, 24, "DONE", keyboard == 43);
    }
    Button(Back, 16, 213, 80, 25, "CANCEL", false);
    set_text_color(TEXT_YELLOW);
    std::string footer = editorNotice.empty() ? Number(draft.size()) + (editorField == 0 ? "/63" : editorField == 4 ? "/5" : "/47") : editorNotice;
    race_mods_text_fit_center(204, 230, footer.c_str(), .45f, 196);
}
void DrawSession(const NetMenuState& state) {
    const bool standings = page == Page::Standings;
    NetPartyInfo leader{}; net_party_info(0, &leader);
    Title(standings ? (state.cupComplete ? "CUP STANDINGS" : "RACE STANDINGS") : state.localOnly ? "LOCAL LOBBY" : "ONLINE LOBBY");
    std::array<NetRosterInfo, 8> roster{};
    int count = 0;
    for (int i = 0; i < 8 && count < 8; ++i)
        if (net_roster_info(i, &roster[count])) ++count;
    if (standings) std::stable_sort(roster.begin(), roster.begin() + count, [](const auto& a, const auto& b) {
        if (a.points != b.points) return a.points > b.points;
        return a.rank < b.rank;
    });
    for (int i = 0; i < count; ++i) {
        const auto& player = roster[i];
        int x = 16 + (i / 4) * 150, y = 44 + (i % 4) * 31;
        Button(player.local && !standings ? 400 + player.controller : 2000 + i, x, y, 138, 29,
               "", player.local && !standings && selected == player.controller, player.local && !standings);
        set_text_color(player.ready || standings ? TEXT_YELLOW : TEXT_GREEN);
        std::string label = (standings ? Number(i + 1) + ". " : "") + PlayerName(player);
        race_mods_text_fit(x + 5, y + 12, label.c_str(), .47f, 126);
        std::string detail = standings ? Number(player.points) + " PTS / " + (player.finished ? Number(player.rank + 1) : "DNF")
            : std::string(player.local ? "LOCAL " : "REMOTE ") + (player.ready ? "READY" : "WAITING");
        race_mods_text_fit(x + 5, y + 24, detail.c_str(), .38f, 126);
    }
    set_text_color(TEXT_YELLOW);
    std::string summary = std::string(Modes[std::clamp(state.mode, 0, 2)]) + " / " +
        (state.mode == 0 ? Cups[std::clamp(state.cup, 0, 3)] : Course(state.course));
    race_mods_text_fit_center(160, 178, summary.c_str(), .46f, 284);
    std::string seats = Number(state.playerCount) + "/" + Number(state.maxPlayers) + " PLAYERS";
    if (state.mode == 0) seats += " / ROUND " + Number(state.raceIndex + 1);
    race_mods_text_fit_center(160, 190, seats.c_str(), .4f, 284);
    Status(state);
    Button(Back, 16, 212, 66, 26, "LEAVE", selected == 4);
    Button(1004, 88, 212, 80, 26, standings ? "LOBBY" : leader.ready ? "NOT READY" : "READY", selected == 5,
           standings ? state.host != 0 : Phase(state, KartNet::Phase::Lobby));
    Button(1006, 174, 212, 58, 26, "SETUP", selected == 6, state.host && !standings && Phase(state, KartNet::Phase::Lobby));
    Button(Primary, 238, 212, 66, 26, standings ? (state.cupComplete ? "NEW CUP" : "NEXT RACE") : "START", selected == 7,
           state.host && state.canStart);
}
const char* PageTitle() {
    switch (page) {
        case Page::Online: return "ONLINE";
        case Page::HostSetup: return "HOST SETUP";
        case Page::LocalSetup: return singlePlayer ? "1P RACE SETUP" : "LOCAL RACE SETUP";
        case Page::Browser: return "LAN SERVER BROWSER";
        case Page::Direct: return "DIRECT CONNECT";
        case Page::Party: return "LOCAL DRIVER";
        default: return "SPAGHETTI KART";
    }
}
const char* PageName() {
    switch (page) {
        case Page::Main: return "MAIN";
        case Page::Lobby: return "LOBBY";
        case Page::Standings: return "STANDINGS";
        case Page::Editor:
            switch (editorField) {
                case 0: return "HOST ADDRESS";
                case 1: return "SERVER NAME";
                case 2: return "SERVER PASSWORD";
                case 3: return "JOIN PASSWORD";
                case 4: return "UDP PORT";
                default: return "TEXT EDITOR";
            }
        default: return PageTitle();
    }
}
void DrawRows(const NetMenuState& state) {
    Rows(state); Title(PageTitle());
    const int first = GameUI::Lobby::PageFirst(selected);
    int end = std::min(first + 6, int(rows.size()));
    for (int i = first; i < end; ++i) {
        const auto& row = rows[i];
        int y = 43 + (i - first) * 25;
        Button(i * 3, 16, y, 288, 24, "", i == selected, row.enabled);
        set_text_color(row.enabled ? TEXT_YELLOW : TEXT_RED);
        const int baseline = row.detail.empty() ? 16 : 11;
        race_mods_text_fit(22, y + baseline, row.label.c_str(), row.detail.empty() ? .57f : .47f, row.value.empty() ? 270 : 144);
        if (!row.value.empty()) race_mods_text_fit_center(236, y + baseline, row.value.c_str(), .49f, 103);
        if (!row.detail.empty()) race_mods_text_fit(22, y + 22, row.detail.c_str(), .34f, 275);
        if (row.enabled && Adjustable(row.action)) {
            race_mods_draw_pointer_arrow(177, y + 11, -1);
            race_mods_draw_pointer_arrow(296, y + 11, 1);
            menu_pointer_target(Context(), i * 3 + 1, 164, y, 26, 24);
            menu_pointer_target(Context(), i * 3 + 2, 283, y, 21, 24);
        }
    }
    Button(Back, 16, 212, 78, 26, "BACK", false);
    if (page == Page::Party) {
        if (!singlePlayer || !state.localOnly) {
            Button(1004, 103, 212, 49, 26, "", false);
            Button(1005, 168, 212, 49, 26, "", false);
            race_mods_draw_pointer_arrow(127, 225, -1); race_mods_draw_pointer_arrow(192, 225, 1);
        }
        set_text_color(TEXT_YELLOW);
        race_mods_text_fit_center(160, 203, ("PLAYER " + Number(partyController + 1)).c_str(), .5f, 150);
    } else {
        if (first > 0) { Button(Previous, 106, 212, 44, 26, "", false); race_mods_draw_pointer_arrow(128, 225, -1); }
        if (end < int(rows.size())) { Button(Next, 170, 212, 44, 26, "", false); race_mods_draw_pointer_arrow(192, 225, 1); }
        Status(state);
    }
    if (page == Page::Online || page == Page::Browser) {
        set_text_color(TEXT_YELLOW);
        race_mods_text_fit_center(160, 35, "LAN DISCOVERY / INTERNET BY ADDRESS", .36f, 280);
    }
    if (page == Page::Browser && rows.size() == 1) {
        set_text_color(TEXT_YELLOW); race_mods_text_fit_center(160, 112, "NO LAN SERVERS FOUND", .6f, 274);
    }
}
} // namespace

extern "C" int game_lobby_is_open(void) { return Visible(); }
extern "C" const char* game_lobby_page(void) { return Visible() ? PageName() : ""; }
extern "C" int game_lobby_context(void) { return Visible() ? Context() : -1; }
extern "C" int game_lobby_row_count(void) {
    return Visible() && HasRows(page) && rowsPage == page ? static_cast<int>(rows.size()) : 0;
}
extern "C" int game_lobby_row_info(int index, GameOptionRowInfo* info) {
    if (!info) return 0;
    *info = {};
    if (index < 0 || index >= game_lobby_row_count()) return 0;
    const auto& row = rows[index];
    std::snprintf(info->label, sizeof(info->label), "%s", row.label.c_str());
    const auto value = row.value + (row.detail.empty() ? "" : " / " + row.detail);
    std::snprintf(info->value, sizeof(info->value), "%s", value.c_str());
    info->index = index; info->enabled = row.enabled; info->adjustable = Adjustable(row.action);
    const int first = GameUI::Lobby::PageFirst(selected);
    info->visible = index >= first && index < first + 6;
    return 1;
}
extern "C" void game_lobby_open_online(void) {
    singlePlayer = false; net_party_set_limit(4); EnsureParty(); Change(Page::Online);
    if (gGamestate != RACING && !MainMenu()) {
        gMenuSelection = MAIN_MENU; gMainMenuSelection = MAIN_MENU_PLAYER_SELECT; gFadeModeSelection = FADE_MODE_MAIN;
    }
}
extern "C" void game_lobby_request_back(void) { backRequested = true; }
extern "C" void game_lobby_scroll(int pages) {
    if (HasRows(page)) {
        selected = std::max(0, selected + pages * 6);
    }
}
extern "C" void game_lobby_net_input(unsigned int buttons, int x, int y) {
    networkPad.buttonPressed = buttons & ~networkPad.button; networkPad.button = buttons;
    unsigned short stick = (x < -30 ? L_JPAD : x > 30 ? R_JPAD : 0) | (y < -30 ? D_JPAD : y > 30 ? U_JPAD : 0);
    networkPad.stickPressed = stick & ~networkPad.stickDirection; networkPad.stickDirection = stick;
}
extern "C" int game_lobby_input(void) {
    if (!Visible()) return 0;
    NetMenuState state{}; net_menu_state(&state);
    if (state.active) {
        sawSession = true;
        if (Phase(state, KartNet::Phase::Finished) && page != Page::Standings && page != Page::Party) Change(Page::Standings);
        else if (SessionPage(state) && !Phase(state, KartNet::Phase::Finished) &&
                 page != Page::Lobby && page != Page::Party && page != Page::Editor &&
                 page != Page::HostSetup && page != Page::LocalSetup) Change(Page::Lobby);
    } else if (sawSession && (page == Page::Lobby || page == Page::Standings)) {
        // Connection errors remain on screen with the backend's status and a Back route.
        if (!Phase(state, KartNet::Phase::Error) && !Phase(state, KartNet::Phase::Disconnected) &&
            !Phase(state, KartNet::Phase::Connecting)) { sawSession = false; Change(Page::Main); }
    }
    if (MainMenu() && !is_screen_being_faded()) gMainMenuSelection = MAIN_MENU_PLAYER_SELECT;
    if (is_screen_being_faded()) return 1;
    unsigned short buttons = kart_input_get_menu_pressed(gGamestate == RACING ? &networkPad : &gControllers[0]);
    networkPad.buttonPressed = networkPad.stickPressed = 0;
    if (HasRows(page)) Rows(state);
    int hit = menu_pointer_take(Context());
    if (backRequested || hit == Back) { backRequested = false; buttons = B_BUTTON; }
    if (page == Page::Main) {
        net_party_set_limit(4);
        for (int i = 1; i < 4; ++i) PartyInput(i, kart_input_get_menu_pressed(&gControllers[i]), false);
        MainInput(buttons, hit); return 1;
    }
    if (page == Page::Party && partyController > 0 && gGamestate != RACING)
        buttons |= kart_input_get_menu_pressed(&gControllers[partyController]);
    if (buttons & B_BUTTON) { BackPage(state); return 1; }
    if (page == Page::Editor) { EditorInput(buttons, hit); return 1; }
    if (gGamestate != RACING) {
        for (int i = 1; i < 4; ++i) {
            if (page == Page::Party && partyController == i) continue;
            PartyInput(i, kart_input_get_menu_pressed(&gControllers[i]), page == Page::Lobby && Phase(state, KartNet::Phase::Lobby));
        }
    }
    if (page == Page::Lobby || page == Page::Standings) { SessionInput(buttons, hit, state); return 1; }
    if (page == Page::Party) {
        if (hit == 1004 || hit == 1005 || (buttons & START_BUTTON)) {
            partyController = Wrap(partyController + (hit == 1004 ? -1 : 1), singlePlayer && state.localOnly ? 1 : 4);
            selected = 0; ++generation;
            menu_pointer_cancel();
        }
    }
    Rows(state);
    if (hit == Previous || hit == Next) selected = std::clamp(selected + (hit == Previous ? -6 : 6), 0, std::max(0, int(rows.size()) - 1));
    else if (hit >= 0 && hit < int(rows.size()) * 3) {
        selected = hit / 3; buttons = hit % 3 == 1 ? L_JPAD : hit % 3 == 2 ? R_JPAD : A_BUTTON;
    }
    if (buttons & U_JPAD) selected = Wrap(selected - 1, int(rows.size()));
    if (buttons & D_JPAD) selected = Wrap(selected + 1, int(rows.size()));
    if (!rows.empty()) {
        const Row row = rows[selected];
        if (buttons & L_JPAD) Activate(row, -1, state);
        else if (buttons & R_JPAD) Activate(row, 1, state);
        else if (buttons & A_BUTTON) Activate(row, 0, state);
    }
    return 1;
}
extern "C" void game_lobby_draw(void) {
    if (!Visible()) return;
    NetMenuState state{}; net_menu_state(&state);
    menu_pointer_clear_targets();
    race_mods_draw_native_backdrop(0, page == Page::Main ? 0 : 100);
    if (page == Page::Main) DrawMain();
    else if (page == Page::Editor) DrawEditor();
    else if (page == Page::Lobby || page == Page::Standings) DrawSession(state);
    else DrawRows(state);
}
