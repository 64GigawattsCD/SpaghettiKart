#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* Shared native pause/options menu; action codes retain the stock transitions. */
int vr_pause_menu_input(void);
void vr_pause_menu_draw(void);
void vr_pause_menu_draw_entry(void);
int vr_pause_menu_is_open(void);
#ifdef __cplusplus
}
#endif
