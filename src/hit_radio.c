#include <libultraship.h>
#include <mk64.h>
#include <defines.h>
#include <assets/models/common_data.h>
#include <assets/models/data_segment2.h>
#include <assets/textures/player_selection.h>
#include "hit_radio.h"
#include "hit_radio_model.h"
#include "main.h"
#include "code_800029B0.h"
#include "render_objects.h"
#include "port/AndroidPhone.h"
#include "port/Engine.h"
#include "port/vr/vr.h"

static HitRadioModel sRadio;
static const char* const sFaces[] = {
    gTextureMarioFace00, gTextureLuigiFace00, gTextureYoshiFace00, gTextureToadFace00,
    gTextureDkFace00, gTextureWarioFace00, gTexturePeachFace00, gTextureBowserFace00
};

void hit_radio_reset(void) {
    int i;
    hit_radio_model_reset(&sRadio);
    if (gGamestate == RACING && !gDemoMode && !gIsInQuitToMenuTransition) {
        for (i = 0; i < 8; ++i) ResourceGetDataByName(sFaces[i]);
    }
}

int hit_radio_show(int localVictimSlot, int attackerCharacter) {
    if (gGamestate != RACING || gDemoMode || gIsInQuitToMenuTransition) return 0;
    if (localVictimSlot < 0 || localVictimSlot >= HIT_RADIO_SLOTS || localVictimSlot >= gPlayerCountSelection1 ||
        attackerCharacter < 0 || attackerCharacter >= 8) return 0;
    if (sRadio.slots[localVictimSlot].cooldown > 0 || !ResourceGetDataByName(sFaces[attackerCharacter])) return 0;
    return hit_radio_model_show(&sRadio, gPlayerCountSelection1, localVictimSlot, attackerCharacter);
}

void hit_radio_tick(float deltaSeconds) {
    hit_radio_model_tick(&sRadio, deltaSeconds);
}

static void radio_box(HudRect rect, HudRect clip, int r, int g, int b, int alpha) {
    HudRect visible = hit_radio_clip(rect, clip);
    int x = (int) ceilf(visible.x), y = (int) ceilf(visible.y);
    int endX = (int) floorf(visible.x + visible.w), endY = (int) floorf(visible.y + visible.h);
    if (x >= endX || y >= endY) return;
    // Same translucent primitive surface as the native menus, with signed x.
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_02008008);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, r, g, b, alpha);
    gDPFillWideRectangle(gDisplayListHead++, x, y, endX, endY);
    gDPPipeSync(gDisplayListHead++);
}

static void radio_portrait(HudRect rect, HudRect clip, const char* texture) {
    int half;
    int x = (int) ceilf(rect.x), y = (int) ceilf(rect.y);
    int size = (int) floorf(rect.w);
    if (size < 2) return;
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D008108);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D007EF8);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
    // Load each 4 KiB half directly by asset name, never through the menu heap.
    for (half = 0; half < 2; ++half) {
        int top = y + size * half / 2, bottom = y + size * (half + 1) / 2;
        HudRect tile = hud_layout_rect(x, top, size, bottom - top);
        HudRect visible = hit_radio_clip(tile, clip);
        int left = (int) ceilf(visible.x), startY = (int) ceilf(visible.y);
        int endX = (int) floorf(visible.x + visible.w), endY = (int) floorf(visible.y + visible.h);
        int ds = (64 << 10) / size, dt = (32 << 10) / (bottom - top);
        int s = ((left - x) * ds) >> 5;
        int t = (half * 32 << 5) + (((startY - top) * dt) >> 5);
        if (left >= endX || startY >= endY) continue;
        gDPLoadTextureTile(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, 64, 64,
                           0, half * 32, 63, half * 32 + 31, 0, G_TX_CLAMP, G_TX_CLAMP,
                           G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPWideTextureRectangle(gDisplayListHead++, left * 4, startY * 4, (endX - 1) * 4, (endY - 1) * 4,
                                G_TX_RENDERTILE, s, t, ds * 4, dt);
    }
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D007EB8);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_BILERP);
}

void hit_radio_draw(int playerId) {
    static const unsigned char accents[8][3] = {
        {232, 80, 72}, {80, 200, 112}, {120, 216, 96}, {112, 168, 240},
        {208, 168, 104}, {240, 216, 80}, {240, 152, 192}, {208, 192, 88}
    };
    HudRaceLayout hud;
    HitRadioLayout layout;
    HudRect panel, portrait;
    float scale, reveal;
    int character, i;
    if (gGamestate != RACING || gDemoMode || gIsInQuitToMenuTransition || playerId < 0 ||
        playerId >= gPlayerCountSelection1 || playerId >= HIT_RADIO_SLOTS) return;
    if (!sRadio.slots[playerId].active) return;
    reveal = hit_radio_model_reveal(&sRadio.slots[playerId]);
    if (reveal <= 0) return;
    character = sRadio.slots[playerId].character;
    get_race_hud_layout(playerId, &hud);
    layout = hit_radio_model_layout(hud, reveal, android_phone_active(),
                                    vr_is_active() ? 4.0f / 3.0f : OTRGetAspectRatio());
    panel = layout.panel;
    scale = layout.scale;
    radio_box(panel, layout.clip, 16, 18, 22, 152);
    radio_box(hud_layout_rect(panel.x, panel.y, panel.w, fmaxf(1, scale)), layout.clip, 255, 232, 112, 152);
    radio_box(hud_layout_rect(panel.x, panel.y + scale, fmaxf(1, scale), panel.h - scale), layout.clip,
              accents[character][0], accents[character][1], accents[character][2], 192);
    portrait = hud_layout_rect(panel.x + 4 * scale, panel.y + 4 * scale, 40 * scale, 40 * scale);
    radio_portrait(portrait, layout.clip, sFaces[character]);
    for (i = 0; i < 3; ++i) {
        float height = (6 + i * 5) * scale;
        radio_box(hud_layout_rect(panel.x + (50 + i * 6) * scale, panel.y + 34 * scale - height,
                                  fmaxf(1, 3 * scale), height), layout.clip, 240, 216, 96, 168);
    }
    // Restore the shared HUD baseline; no matrices, text globals, or scissor changed.
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D007EB8);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_BILERP);
}
