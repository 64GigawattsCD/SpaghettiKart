#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void net_read_inputs(void);
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
} NetMenuState;
void net_menu_state(NetMenuState* state);
void net_menu_adjust(int field, int direction);
void net_menu_command(int action);
#ifdef __cplusplus
}
#include <nlohmann/json.hpp>
namespace KartNet { nlohmann::json DebugState(); void DrawWindow(); }
#endif
