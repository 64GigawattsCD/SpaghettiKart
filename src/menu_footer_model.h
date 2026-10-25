#pragma once

enum MenuFooterAction { FOOTER_NONE, FOOTER_OPTIONS, FOOTER_DATA, FOOTER_BACK, FOOTER_QUIT, FOOTER_CONSUMED };
enum MenuFooterNavigation { FOOTER_LEFT = 1, FOOTER_RIGHT = 2, FOOTER_UP = 4, FOOTER_DOWN = 8,
                            FOOTER_CANCEL = 16, FOOTER_CONFIRM = 32 };
enum { MENU_FOOTER_X = 16, MENU_FOOTER_Y = 202, MENU_FOOTER_WIDTH = 68,
       MENU_FOOTER_HEIGHT = 28, MENU_FOOTER_PITCH = 74, MENU_FOOTER_FIRST_ID = 301 };

/* -1 leaves the player-count tiles in charge; 0..3 focus the footer. */
static inline int menu_footer_step(int* selected, int playerCount, unsigned int navigation, int pointerId) {
    if (pointerId >= MENU_FOOTER_FIRST_ID && pointerId < MENU_FOOTER_FIRST_ID + 4) {
        *selected = pointerId - MENU_FOOTER_FIRST_ID;
        return FOOTER_OPTIONS + *selected;
    }
    if (pointerId >= 0 && pointerId < 4) {
        *selected = -1;
        return FOOTER_NONE;
    }
    if (*selected < 0) {
        if (!(navigation & FOOTER_DOWN)) return FOOTER_NONE;
        *selected = playerCount >= 1 && playerCount <= 4 ? playerCount - 1 : 0;
    } else if (navigation & (FOOTER_UP | FOOTER_CANCEL)) {
        *selected = -1;
    } else if (navigation & FOOTER_LEFT) {
        *selected = (*selected + 3) % 4;
    } else if (navigation & FOOTER_RIGHT) {
        *selected = (*selected + 1) % 4;
    } else if (navigation & FOOTER_CONFIRM) {
        return FOOTER_OPTIONS + *selected;
    }
    return FOOTER_CONSUMED;
}
