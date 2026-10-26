#include <libultraship.h>
#include <defines.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "main.h"
#include "code_800029B0.h"
#include "hit_radio.h"
#include "hit_radio_model.h"

s32 gGamestate = RACING;
s32 gPlayerCountSelection1 = 4;
u16 gDemoMode;
u16 gIsInQuitToMenuTransition;
static Gfx commands[4096];
Gfx* gDisplayListHead = commands;
static int loads, missing, phone, vr, screenMode;
static float aspect = 4.0f / 3;
static const char* lastLoad;
static int checks;

void gSPDisplayList(Gfx* packet, Gfx* list) {
    __gSPDisplayList(packet, list);
}

static void check(int pass, const char* message) {
    ++checks;
    if (!pass) { fprintf(stderr, "%s\n", message); exit(1); }
}
void* ResourceGetDataByName(const char* name) {
    ++loads;
    lastLoad = name;
    return missing ? NULL : commands;
}
int android_phone_active(void) { return phone; }
int vr_is_active(void) { return vr; }
float OTRGetAspectRatio(void) { return aspect; }
void get_race_hud_layout(s32 playerId, HudRaceLayout* result) {
    *result = hud_race_layout(hud_race_viewport(vr ? 4.0f / 3 : aspect, screenMode, playerId),
                              1, hud_layout_vec2(64, 64), true, true);
}
static int signed_bits(uintptr_t word, int bits) {
    unsigned int mask = (1u << bits) - 1;
    unsigned int number = (unsigned int) word & mask;
    return number & (1u << (bits - 1)) ? (int) number - (int) (1u << bits) : (int) number;
}
static void command_bounds(int player, int held) {
    Gfx* cmd;
    HudRaceLayout hud;
    int textures = 0, boxes = 0;
    get_race_hud_layout(player, &hud);
    for (cmd = commands; cmd < gDisplayListHead; ++cmd) {
        int op = (int) (cmd->words.w0 >> 24) & 255;
        float left, top, endX, endY;
        check(op != G_SETSCISSOR && op != G_MTX && op != G_MOVEMEM, "global scissor/matrix/viewport changed");
        if (op == G_TEXRECT_WIDE) {
            endX = signed_bits(cmd->words.w0, 24) / 4.0f + 1;
            endY = signed_bits(cmd->words.w1, 24) / 4.0f + 1;
            left = signed_bits(cmd[1].words.w0, 24) / 4.0f;
            top = signed_bits(cmd[1].words.w1, 24) / 4.0f;
            check(((cmd[2].words.w1 >> 16) & 65535) > 0 && (cmd[2].words.w1 & 65535) > 0,
                   "zero texture step");
            cmd += 2;
            ++textures;
        } else if (op == G_FILLWIDERECT) {
            endX = signed_bits(cmd->words.w0, 24) / 4.0f;
            endY = signed_bits(cmd->words.w1, 24) / 4.0f;
            left = signed_bits(cmd[1].words.w0, 24) / 4.0f;
            top = signed_bits(cmd[1].words.w1, 24) / 4.0f;
            ++cmd;
            ++boxes;
        } else continue;
        check(left >= hud.viewport.x - .001f && endX <= hud.viewport.x + hud.viewport.w + .001f &&
               top >= hud.viewport.y - .001f && endY <= hud.viewport.y + hud.viewport.h + .001f,
               "emitted rectangle crosses viewport");
        check(left < endX && top < endY, "degenerate draw");
    }
    if (held) {
        check(boxes > 0, "missing native frame");
        check(textures == 2, "held stock portrait must have both halves");
    }
    // A partly revealed panel may not yet expose the portrait.
    check(textures <= 2, "portrait not bounded to two tiles");
}
static int draw(int player) {
    gDisplayListHead = commands;
    hit_radio_draw(player);
    check(gDisplayListHead < commands + 4096, "display list budget");
    return gDisplayListHead != commands;
}
int main(void) {
    int i, mode, player, frame;
    const float aspects[] = {1, 4.0f / 3, 16.0f / 9, 2.17f, 32.0f / 9};
    const char* names[] = {"mario", "luigi", "yoshi", "toad", "donkeykong", "wario", "peach", "bowser"};
    hit_radio_reset();
    check(loads == 8, "race reset preloads eight stock faces");
    check(!hit_radio_show(-1, 0) && !hit_radio_show(4, 0) && !hit_radio_show(0, -1) && !hit_radio_show(0, 8) &&
           !hit_radio_show(INT_MIN, INT_MAX), "C API invalid inputs");
    for (i = 0; i < 8; ++i) {
        hit_radio_reset();
        check(hit_radio_show(2, i), "C API character acceptance");
        check(strstr(lastLoad, names[i]) && strstr(lastLoad, "face_00"), "stock face order");
    }
    hit_radio_reset();
    check(hit_radio_show(2, 0), "C API initial acceptance");
    check(!draw(2) && !draw(0), "initial/victim only draw");
    hit_radio_tick(.15f);
    check(draw(2) && !draw(1) && !draw(-1) && !draw(4), "C API draw slot checks");
    check(!hit_radio_show(2, 1), "C API cooldown rejects");
    hit_radio_tick(.60f);
    check(hit_radio_show(2, 1), "C API cooldown accepts");
    missing = 1;
    check(!hit_radio_show(0, 0), "missing resource not accepted");
    missing = 0;
    check(hit_radio_show(0, 0), "missing resource leaves slot available");
    hit_radio_tick(2);
    check(!draw(0) && !draw(2), "C API expires without queue");
    gGamestate = MAIN_MENU_FROM_QUIT;
    loads = 0;
    hit_radio_reset();
    check(!loads && !hit_radio_show(0, 0) && !draw(0), "menus clear stale event");
    gGamestate = RACING;
    gDemoMode = 1;
    check(!hit_radio_show(0, 0), "demo rejection");
    gDemoMode = 0;
    gIsInQuitToMenuTransition = 1;
    check(!hit_radio_show(0, 0), "quit rejection");
    gIsInQuitToMenuTransition = 0;
    for (i = 0; i < sizeof(aspects) / sizeof(aspects[0]); ++i) {
        aspect = aspects[i];
        for (mode = 0; mode < 4; ++mode) {
            screenMode = mode;
            gPlayerCountSelection1 = mode == 0 ? 1 : mode == 3 ? 4 : 2;
            for (phone = 0; phone < 2; ++phone) {
                for (player = 0; player < gPlayerCountSelection1; ++player) {
                    hit_radio_reset();
                    check(hit_radio_show(player, player), "render event accepted");
                    for (frame = 0; frame < 185; ++frame) {
                        if (draw(player)) command_bounds(player, frame == 15);
                        hit_radio_tick(.01f);
                    }
                }
            }
        }
    }
    vr = 1;
    hit_radio_reset();
    hit_radio_show(0, 0);
    hit_radio_tick(.15f);
    check(draw(0), "VR HUD plane draw");
    command_bounds(0, 1);
    // A narrow reference viewport yields 0/1-pixel portraits; neither half divides by zero.
    vr = 0; phone = 1; screenMode = 0; gPlayerCountSelection1 = 1; aspect = .12f;
    hit_radio_reset(); hit_radio_show(0, 0); hit_radio_tick(.15f); draw(0);
    printf("%d radio API/render checks passed\n", checks);
    return 0;
}
