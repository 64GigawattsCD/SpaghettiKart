#include "vr_pause_menu.h"
#include "game_options.h"

/* Compatibility entry points for the native pause state machine and VR HUD pass. */
int vr_pause_menu_input(void) { return game_options_pause_input(); }
void vr_pause_menu_draw(void) { game_options_draw(); }
void vr_pause_menu_draw_entry(void) {}
int vr_pause_menu_is_open(void) { return game_options_is_open(); }
