// Native footer: directional focus and pointer activation share the same actions.
#include <libultraship.h>
#include <libultra/types.h>
#include <macros.h>
#include <mk64.h>
#include <defines.h>
#include <sounds.h>
#include "menu_items.h"
#include "menus.h"
#include "audio/external.h"
#include "quit_menu.h"
#include "menu_pointer.h"
#include "race_mods_menu.h"
#include "game_options.h"
#include "menu_footer_model.h"

extern Gfx* gDisplayListHead;

// Clean app quit bridge (src/port/Engine.cpp): backend mIsRunning = false -> SDL_main's
// while (WindowIsRunning()) exits -> CustomEngineDestroy() + GameEngine::Destroy().
extern void GameEngine_RequestQuit(void);

// Frames until the actual quit. The native wipe started in quit_menu_trigger runs 20 frames, so the app
// exits mid-wipe (flash + click + fade-out are all visible) and the wipe's completion dispatch never runs.
static s32 sQuitCountdown = 0;
static s32 sConfirm = 0;
static s32 sConfirmYes = 0;
static int sFooterSelection = -1;

static s32 quit_pointer_context(void) {
    return 150000 + gMainMenuSelection * 2 + sConfirm;
}

int quit_menu_input(unsigned short btnAndStick) {
    if (sQuitCountdown > 0) {
        return 1; // quit pending - the wipe gates input too, but don't let the stock chain race it
    }
    s32 hit = menu_pointer_take(quit_pointer_context());
    if (sConfirm) {
        if (hit == 1 || hit == 2) {
            sConfirmYes = hit == 1;
            btnAndStick = A_BUTTON;
        }
        if (btnAndStick & (L_JPAD | R_JPAD)) {
            sConfirmYes = !sConfirmYes;
            reset_cycle_flash_menu();
            play_sound2(SOUND_MENU_CURSOR_MOVE);
        }
        if (btnAndStick & A_BUTTON) {
            sConfirm = 0;
            menu_pointer_cancel();
            if (sConfirmYes) quit_menu_trigger();
            else play_sound2(SOUND_MENU_GO_BACK);
        } else if (btnAndStick & B_BUTTON) {
            sConfirm = 0;
            menu_pointer_cancel();
            play_sound2(SOUND_MENU_GO_BACK);
        }
        return 1; // dialog owns all input while it's up (dpad must not move the selection behind it)
    }
    return 0;
}

int main_menu_footer_focused(void) {
    return gMainMenuSelection == MAIN_MENU_PLAYER_SELECT && sFooterSelection >= 0;
}

int main_menu_footer_input(unsigned short* buttons, int pointerId) {
    unsigned int navigation = 0;
    int previous = sFooterSelection;
    if (*buttons & L_JPAD) navigation |= FOOTER_LEFT;
    if (*buttons & R_JPAD) navigation |= FOOTER_RIGHT;
    if (*buttons & U_JPAD) navigation |= FOOTER_UP;
    if (*buttons & D_JPAD) navigation |= FOOTER_DOWN;
    if (*buttons & B_BUTTON) navigation |= FOOTER_CANCEL;
    if (*buttons & (A_BUTTON | START_BUTTON)) navigation |= FOOTER_CONFIRM;
    int action = menu_footer_step(&sFooterSelection, gPlayerCount, navigation, pointerId);
    if (previous != sFooterSelection) {
        reset_cycle_flash_menu();
        play_sound2(SOUND_MENU_CURSOR_MOVE);
    }
    switch (action) {
        case FOOTER_OPTIONS:
            game_options_request();
            play_sound2(SOUND_MENU_OPTION);
            break;
        case FOOTER_DATA:
            sFooterSelection = -1;
            gMainMenuSelection = MAIN_MENU_DATA;
            func_8009E258();
            play_sound2(SOUND_MENU_DATA);
            break;
        case FOOTER_BACK:
            sFooterSelection = -1;
            *buttons = B_BUTTON;
            return 0;
        case FOOTER_QUIT:
            sConfirm = 1;
            sConfirmYes = 0;
            menu_pointer_cancel();
            play_sound2(SOUND_MENU_SELECT);
            break;
        case FOOTER_NONE: return 0;
    }
    return 1;
}

void quit_menu_trigger(void) {
    if (sQuitCountdown != 0) {
        return;
    }
    sQuitCountdown = 15;
    play_sound2(SOUND_MENU_OK_CLICKED);
    func_800CA330(0x19);                   // fade the menu music out (same call as the B-back path)
    gMenuFadeType = MENU_FADE_TYPE_OPTION; // belt + braces: if the wipe ever completed, it lands on Options
    func_8009DFE0(20);                     // native colored menu wipe; also gates all menu input via
                                           // is_screen_being_faded() so nothing else can fire meanwhile
}

void quit_menu_update(void) {
    if (gMainMenuSelection != MAIN_MENU_PLAYER_SELECT) sFooterSelection = -1;
    if (sQuitCountdown > 0 && --sQuitCountdown == 0) {
        GameEngine_RequestQuit();
    }
}

void main_menu_footer_draw(void) {
    static const char* labels[] = { "OPTIONS", "DATA", "BACK", "QUIT" };
    if (gMainMenuSelection != MAIN_MENU_PLAYER_SELECT) return;
    for (int i = 0; i < 4; ++i) {
        int x = MENU_FOOTER_X + i * MENU_FOOTER_PITCH;
        race_mods_draw_native_button(x, MENU_FOOTER_Y, MENU_FOOTER_WIDTH, MENU_FOOTER_HEIGHT,
                                    labels[i], sFooterSelection == i);
        if (!sConfirm && !sQuitCountdown && !is_screen_being_faded())
            menu_pointer_target(menu_pointer_main_context(), MENU_FOOTER_FIRST_ID + i,
                                x, MENU_FOOTER_Y, MENU_FOOTER_WIDTH, MENU_FOOTER_HEIGHT);
    }
}

void quit_menu_draw_confirm(void) {
    if (!sConfirm) {
        return;
    }
    menu_pointer_clear_targets();
    // Drawn from the tail of the menu-item render pass, so the dialog covers the player-select widgets.
    gDisplayListHead = draw_box_wide(gDisplayListHead, 0, 0, 320, 240, 0, 0, 0, 120);
    gDisplayListHead = draw_box_fill(gDisplayListHead, 72, 86, 248, 156, 0, 0, 0, 255);
    set_text_color(TEXT_YELLOW);
    race_mods_text_fit_center(160, 112, "QUIT GAME?", .8f, 156);
    race_mods_draw_native_button(86, 125, 72, 26, "YES", sConfirmYes);
    race_mods_draw_native_button(162, 125, 72, 26, "NO", !sConfirmYes);
    menu_pointer_target(quit_pointer_context(), 1, 86, 125, 72, 26);
    menu_pointer_target(quit_pointer_context(), 2, 162, 125, 72, 26);
}
