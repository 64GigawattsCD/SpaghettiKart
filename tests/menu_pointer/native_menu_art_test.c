#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int s32;
typedef float f32;
typedef int Gfx;
enum { TEXT_YELLOW = 1 };
static Gfx* gDisplayListHead;
typedef struct { int x, y, endX, endY, r, g, b, alpha; } Box;
static Box boxes[32];
static int count, textCalls, color, textX, textY;
static float textScale, textWidth;
static const char* textLabel;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static Gfx* draw_box(Gfx* dl, int x, int y, int endX, int endY, int r, int g, int b, int alpha) {
    CHECK(count < 32);
    boxes[count++] = (Box){x, y, endX, endY, r, g, b, alpha};
    return dl;
}
static void set_text_color(int next) { color = next; }
static void set_text_color_rainbow_if_selected(int selected, int target, int fallback) {
    color = selected == target ? 99 : fallback;
}
static void race_mods_text_fit_center(int x, int y, const char* label, float scale, float maxWidth) {
    ++textCalls;
    textX = x; textY = y; textLabel = label; textScale = scale; textWidth = maxWidth;
}

#include "native_menu_art.inc"

static void reset(void) { count = textCalls = 0; color = -1; }
static int covers(Box box, int x, int y) {
    return x >= box.x && x < box.endX && y >= box.y && y < box.endY;
}
static void check_bounds(int x, int y, int width, int height) {
    for (int i = 0; i < count; ++i) {
        Box box = boxes[i];
        CHECK(box.x >= x && box.endX <= x + width && box.x < box.endX);
        CHECK(box.y >= y && box.endY <= y + height && box.y < box.endY);
        CHECK(box.alpha > 0 && box.alpha < 255);
    }
}

int main(void) {
    const int widths[] = {4, 18, 64, 72, 88, 176, 268, 280};
    const int heights[] = {4, 18, 19, 24, 25, 26, 27, 70};
    for (int size = 0; size < 8; ++size) {
        int width = widths[size], height = heights[size];
        for (int selected = 0; selected < 2; ++selected) {
            reset();
            race_mods_draw_native_surface(20, 30, width, height, selected);
            check_bounds(20, 30, width, height);
            CHECK(!textCalls && color == -1);
            for (int i = 0; i < count; ++i) {
                CHECK(!covers(boxes[i], 20, 30));
                CHECK(!covers(boxes[i], 20 + width - 1, 30 + height - 1));
            }
            int interior = 0;
            for (int i = 0; i < count; ++i) {
                if (covers(boxes[i], 20 + width / 2, 30 + height / 2)) {
                    CHECK(boxes[i].alpha == 152);
                    ++interior;
                }
            }
            CHECK(interior == 1);
        }
    }
    reset();
    race_mods_draw_native_focus(20, 30, 88, 27);
    CHECK(count == 4 && !textCalls && color == -1);
    check_bounds(20, 30, 88, 27);
    for (int i = 0; i < count; ++i) CHECK(!covers(boxes[i], 64, 43));
    reset();
    race_mods_draw_native_surface(20, 30, 0, 27, 1);
    race_mods_draw_native_surface(20, 30, 88, -1, 1);
    race_mods_draw_native_focus(20, 30, 2, 2);
    CHECK(!count);
    race_mods_draw_native_surface(20, 30, 2, 2, 1);
    CHECK(count == 1);
    check_bounds(20, 30, 2, 2);
    reset();
    race_mods_draw_native_title(160, 29, "OPTIONS", .9f, 275);
    CHECK(textCalls == 1 && textX == 160 && textY == 29 && !strcmp(textLabel, "OPTIONS"));
    CHECK(color == TEXT_YELLOW && count == 4 && textScale == .9f && textWidth == 275);
    check_bounds(156, 32, 8, 4);
    reset();
    race_mods_draw_native_button(225, 212, 88, 27, "OPTIONS", 2);
    CHECK(textCalls == 1 && textX == 269 && textY == 230 && color == 99);
    CHECK(textWidth == 78 && textScale == .62f && !strcmp(textLabel, "OPTIONS"));
    check_bounds(225, 212, 88, 27);
    puts("Native art: translucent strips, untouched corners, focus-only edges, title and button contracts passed");
    return 0;
}
