#include "../../src/port/ui/GameOptionsModel.h"
#include "../../src/menu_footer_model.h"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace GameUI::Options;
static void Check(bool passed, const char* message) {
    if (!passed) { std::cerr << message << '\n'; std::exit(1); }
}

int main() {
    int footer = -1;
    Check(menu_footer_step(&footer, 1, 0, -1) == FOOTER_NONE, "Idle player navigation remains native");
    Check(menu_footer_step(&footer, 1, FOOTER_CONFIRM, -1) == FOOTER_NONE, "Confirm on player tile proceeds");
    for (int player = 1; player <= 4; ++player) {
        footer = -1;
        Check(menu_footer_step(&footer, player, FOOTER_DOWN, -1) == FOOTER_CONSUMED && footer == player - 1,
              "Down enters footer below the selected player tile");
        Check(menu_footer_step(&footer, player, FOOTER_UP | FOOTER_CONFIRM, -1) == FOOTER_CONSUMED && footer == -1,
              "Up leaves footer without confirming behind it");
    }
    footer = 0;
    Check(menu_footer_step(&footer, 1, FOOTER_LEFT, -1) == FOOTER_CONSUMED && footer == 3, "Footer reverse wrap");
    Check(menu_footer_step(&footer, 1, FOOTER_RIGHT, -1) == FOOTER_CONSUMED && footer == 0, "Footer forward wrap");
    for (int item = 0; item < 4; ++item) {
        footer = item;
        Check(menu_footer_step(&footer, 1, FOOTER_CONFIRM, -1) == FOOTER_OPTIONS + item, "Footer controller action order");
        footer = -1;
        Check(menu_footer_step(&footer, 1, 0, MENU_FOOTER_FIRST_ID + item) == FOOTER_OPTIONS + item && footer == item,
              "Footer pointer action matches controller");
        Check(MENU_FOOTER_X + item * MENU_FOOTER_PITCH + MENU_FOOTER_WIDTH <= 312, "Footer tile fits game image");
    }
    Check(MENU_FOOTER_Y + MENU_FOOTER_HEIGHT <= 238 && MENU_FOOTER_WIDTH < MENU_FOOTER_PITCH, "Footer padding and gaps");
    Check(menu_footer_step(&footer, 1, FOOTER_CANCEL | FOOTER_CONFIRM, -1) == FOOTER_CONSUMED && footer == -1,
          "Cancel returns focus without activating");
    footer = 3;
    Check(menu_footer_step(&footer, 1, FOOTER_CONFIRM, 1) == FOOTER_NONE && footer == -1,
          "Player tile pointer clears footer focus before confirming");
    Check(menu_footer_step(&footer, 1, 0, 999) == FOOTER_NONE, "Unknown pointer does not activate footer");
    for (int count = 0; count <= 180; ++count) {
        Cursor cursor;
        cursor.selected = 1000;
        cursor.Clamp(count);
        Check(cursor.selected == std::max(0, count - 1), "Clamp after dynamically hidden rows");
        for (int i = 0; i < 370; ++i) {
            cursor.Move(i % 2 ? 1 : -1, count);
            Check(cursor.selected >= 0 && cursor.selected < std::max(1, count), "Selection range");
            Check(cursor.First() <= cursor.selected && cursor.selected < cursor.First() + Cursor::PageRows, "Selected row must be on its page");
        }
        cursor.selected = 0;
        cursor.Scroll(-1, count);
        Check(cursor.selected == 0, "Previous on first page");
        cursor.Scroll(1000, count);
        Check(cursor.selected == std::max(0, count - 1), "Partial final page");
    }
    Check(CycleKey({0, 3, 7}, 3, 1) == 7, "Sparse enum next");
    Check(CycleKey({0, 3, 7}, 0, -1) == 7, "Sparse enum reverse wrap");
    Check(CycleKey({0, 3, 7}, 7, 0) == 0, "Confirm cycles enum forward");
    Check(CycleKey({0, 3, 7}, 999, 1) == 0, "Unknown stored enum selects supported key");
    Check(CycleKey({}, 99, 1) == 99, "Empty platform choices");
    Check(Adjust(1.f, 1.f, 0.f, 1.f, .01f, 1) == 1.f, "Float upper clamp");
    Check(Adjust(0.f, 1.f, 0.f, 1.f, .01f, -1) == 0.f, "Float lower clamp");
    Check(std::abs(Adjust(std::numeric_limits<float>::quiet_NaN(), .5f, 0.f, 1.f, .05f, 1) - .55f) < .0001f, "Nonfinite saved float recovery");
    Check(Adjust(.5f, .5f, 0.f, 1.f, 0.f, -1) < .5f, "Invalid step recovery");
    Check(Classify("Settings", "Audio", "") == Page::Audio, "Audio backend route");
    Check(Classify("Enhancements", "General", "gArcadeKart.RpmDisplayMultiplier") == Page::Hud, "RPM belongs with HUD");
    Check(Classify("Enhancements", "General", "gArcadeKart.PostFx.HudOverscanCompensation") == Page::Hud, "HUD compensation precedes effects");
    Check(Classify("Enhancements", "General", "gArcadeKart.Camera.SpeedWideFov") == Page::Camera, "Speed camera route");
    Check(Classify("Enhancements", "General", "gArcadeKart.LogitechSdkSpringPercent") == Page::Controls, "Wheel hardware route");
    Check(Classify("Settings", "General", "gPhone.HudLayout") == Page::Hud, "Phone layout route");
    Check(FontText("Anti-aliasing (MSAA): 4x") == "ANTI-ALIASING MSAA : 4X", "Game font strips unavailable glyphs");
    std::cout << "Native option navigation, classification and stored-value checks passed\n";
}
