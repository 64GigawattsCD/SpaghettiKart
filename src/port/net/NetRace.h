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
#ifdef __cplusplus
}
#include <nlohmann/json.hpp>
namespace KartNet { nlohmann::json DebugState(); void DrawWindow(); }
#endif
