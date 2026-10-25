#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void net_read_inputs(void);
struct Controller;
struct Controller* net_controller_for_racer(int racer);
int net_total_laps(void);
int net_course_index(void);
int net_items_enabled(void);
int net_item_mode(void);
enum NetCapability { NET_CAP_ONLINE_ITEMS = 1 << 0, NET_CAP_ONLINE_BATTLE = 1 << 1 };
unsigned int net_capabilities(void);
int net_world_paused(void);
int net_active(void);
int net_race_active(void);
int net_is_host(void);
int net_running(void);
int net_take_load(void);
void net_configure_race(void);
int net_spawn_players(float x, float z, float y);
int net_before_tick(void);
void net_update_display(void);
void net_after_frame(void);
int net_gear(int player);
float net_rpm(int player);
typedef struct {
    int active, inRace, canStart, character, port;
    char address[64];
    char status[192];
    int phase, host, localOnly, scope, mode, maxPlayers, cup, course, cc, laps, cpuFill, mirror, items, balloons;
    int localCount, playerCount, ready, raceIndex, cupComplete, passwordSet;
    char name[48];
} NetMenuState;
typedef struct {
    int joined, controller, character, transmission, ready;
} NetPartyInfo;
typedef struct {
    int participant, connection, racer, controller, character, transmission, ready, connected, local;
    int rank, points, finished;
} NetRosterInfo;
typedef struct {
    char name[48], address[64];
    int port, mode, cup, course, players, maxPlayers, password, compatible, ping, phase;
} NetServerInfo;
enum NetSettingField {
    NET_SETTING_CHARACTER = 0, NET_SETTING_PORT = 1,
    NET_SETTING_IP_0 = 2, NET_SETTING_IP_1, NET_SETTING_IP_2, NET_SETTING_IP_3,
    NET_SETTING_SCOPE = 6, NET_SETTING_MODE, NET_SETTING_MAX_PLAYERS, NET_SETTING_CUP,
    NET_SETTING_COURSE, NET_SETTING_CC, NET_SETTING_LAPS, NET_SETTING_CPU_FILL,
    NET_SETTING_MIRROR, NET_SETTING_ITEMS, NET_SETTING_BALLOONS
};
enum NetMenuAction {
    NET_HOST = 1, NET_JOIN, NET_START, NET_LEAVE, NET_LOCAL, NET_READY, NET_RETURN_LOBBY, NET_REFRESH
};
enum NetTextField { NET_TEXT_ADDRESS = 0, NET_TEXT_NAME, NET_TEXT_PASSWORD, NET_TEXT_PORT };
void net_menu_state(NetMenuState* state);
void net_menu_adjust(int field, int direction);
void net_menu_command(int action);
int net_party_info(int controller, NetPartyInfo* info);
void net_party_join(int controller);
void net_party_set_limit(int count);
int net_party_character_available(int controller, int character);
int net_party_select_character(int controller, int character);
void net_party_leave(int controller);
void net_party_adjust(int controller, int field, int direction); /* 0 character, 1 transmission, 2 ready */
void net_party_ready(int controller, int ready);
int net_roster_info(int index, NetRosterInfo* info);
void net_menu_text(int field, const char* text);
void net_menu_get_text(int field, char* text, int capacity);
int net_browser_count(void);
int net_browser_info(int index, NetServerInfo* info);
void net_browser_join(int index);
#ifdef __cplusplus
}
#include <nlohmann/json.hpp>
namespace KartNet { nlohmann::json DebugState(); void DrawWindow(); }
#endif
