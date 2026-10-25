#include "hud_race_layout.h"
#include <math.h>

HudRect hud_race_viewport(f32 aspect, s32 screenMode, s32 player) {
    f32 width = 240.0f * ((aspect > 0.0f) ? aspect : 4.0f / 3.0f);
    HudRect view = hud_layout_rect(160.0f - width * 0.5f, 0.0f, width, 240.0f);
    // Screen modes: 0 single, 1 horizontal, 2 vertical, 3 four quadrants.
    if (screenMode == 1 || screenMode == 3) {
        view.h *= 0.5f;
        view.y = ((screenMode == 1) ? player : player / 2) * view.h;
    }
    if (screenMode == 2 || screenMode == 3) {
        view.w *= 0.5f;
        view.x += (player % 2) * view.w;
    }
    return view;
}

HudRaceLayout hud_race_layout(HudRect viewport, f32 marginScale, HudVec2 mapSize, bool laps, bool rpm) {
    HudRaceLayout result = {0};
    HudLayoutContext ctx;
    HudPadding zero = hud_layout_padding(0, 0, 0, 0);
    f32 scale = 0.8f * fminf(viewport.w / 320.0f, viewport.h / 240.0f);
    f32 edge = 8.0f * scale * fmaxf(0.25f, fminf(marginScale, 3.0f));
    f32 gap = 8.0f * scale;
    f32 columnWidth = fminf(96.0f * scale, fmaxf(0, viewport.w - 2 * edge));
    HudWidgetId column, lapBlock, mapContainer, mapBox, map, rpmBlock;
    hud_layout_begin_safe_zone(&ctx, viewport, hud_layout_padding(edge, edge, edge, edge));
    column = hud_layout_vertical_box(&ctx, 0, zero);
    hud_layout_canvas_add(&ctx, hud_layout_root(&ctx), column,
        hud_layout_canvas_safe_slot(hud_layout_anchor(1, 0, 1, 1),
            hud_layout_padding(0, 0, columnWidth, 0), hud_layout_vec2(1, 0), false, 0));
    lapBlock = hud_layout_draw(&ctx, hud_layout_vec2(64 * scale, laps ? 48 * scale : 0), NULL, NULL);
    hud_layout_box_add(&ctx, column, lapBlock,
        hud_layout_auto_slot(hud_layout_padding(0, 0, 0, laps ? gap : 0), hud_layout_vec2(1, 0), false));
    mapContainer = hud_layout_canvas(&ctx);
    hud_layout_box_add(&ctx, column, mapContainer, hud_layout_fill_slot(1, zero, hud_layout_vec2(1, .5f), true));
    mapBox = hud_layout_scale_box(&ctx, HUD_SCALE_STRETCH_FIT, HUD_SCALE_DIRECTION_BOTH, 1);
    // Fit the actual remaining gap. Mirroring the larger corner block's height
    // above and below the map wasted space and shrank tall tracks unnecessarily.
    hud_layout_canvas_add(&ctx, mapContainer, mapBox,
        hud_layout_canvas_slot(hud_layout_anchor(0, 0, 1, 1), zero,
            hud_layout_vec2(0, 0), false, 0));
    // Include room around the track image for kart/finish icons at its perimeter.
    mapSize.x = fmaxf(1, mapSize.x);
    mapSize.y = fmaxf(1, mapSize.y);
    map = hud_layout_draw(&ctx, hud_layout_vec2((mapSize.x + 12) * scale, (mapSize.y + 12) * scale), NULL, NULL);
    hud_layout_single_child_add(&ctx, mapBox, map,
        hud_layout_single_child_slot(zero, hud_layout_vec2(1, .5f), false, false));
    rpmBlock = hud_layout_draw(&ctx, hud_layout_vec2(96 * scale, rpm ? 96 * scale : 0), NULL, NULL);
    hud_layout_box_add(&ctx, column, rpmBlock,
        hud_layout_auto_slot(hud_layout_padding(0, rpm ? gap : 0, 0, 0), hud_layout_vec2(1, 1), false));
    hud_layout_arrange(&ctx);
    result.viewport = viewport;
    result.column = hud_layout_get_rect(&ctx, column);
    result.laps = hud_layout_get_rect(&ctx, lapBlock);
    result.mapSlot = hud_layout_get_rect(&ctx, mapBox);
    result.map = hud_layout_get_rect(&ctx, map);
    result.rpm = hud_layout_get_rect(&ctx, rpmBlock);
    result.scale = scale;
    result.mapScale = result.map.w / (mapSize.x + 12);
    result.map.x += 6 * result.mapScale;
    result.map.y += 6 * result.mapScale;
    result.map.w = mapSize.x * result.mapScale;
    result.map.h = mapSize.y * result.mapScale;
    return result;
}
