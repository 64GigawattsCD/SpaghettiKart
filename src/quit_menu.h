// Main-menu footer and native quit confirmation.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Returns nonzero while the confirmation owns input or shutdown is pending.
int quit_menu_input(unsigned short btnAndStick);
int main_menu_footer_input(unsigned short* btnAndStick, int pointerId);
int main_menu_footer_focused(void);
void main_menu_footer_draw(void);
// Starts the actual quit sequence. Idempotent while a quit is already pending.
void quit_menu_trigger(void);
// Ticks the pending-quit countdown. Called once per frame from the main menu's item update.
void quit_menu_update(void);
// Draws the QUIT confirm dialog. Called after the whole menu-item render pass so it sits on top.
void quit_menu_draw_confirm(void);

#ifdef __cplusplus
}
#endif
