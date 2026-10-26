#ifndef HIT_RADIO_MODEL_H
#define HIT_RADIO_MODEL_H

#include <float.h>
#include <math.h>
#include "hud_race_layout.h"

#define HIT_RADIO_SLOTS 4
#define HIT_RADIO_SLIDE 0.15f
#define HIT_RADIO_HOLD 1.5f
#define HIT_RADIO_DURATION (HIT_RADIO_SLIDE + HIT_RADIO_HOLD + HIT_RADIO_SLIDE)
#define HIT_RADIO_COOLDOWN 0.75f

typedef struct HitRadioSlot {
    float age;
    float cooldown;
    int character;
    int active;
} HitRadioSlot;

typedef struct HitRadioModel {
    HitRadioSlot slots[HIT_RADIO_SLOTS];
} HitRadioModel;

typedef struct HitRadioLayout {
    HudRect clip;
    HudRect panel;
    float scale;
} HitRadioLayout;

static inline void hit_radio_model_reset(HitRadioModel* model) {
    int i;
    for (i = 0; i < HIT_RADIO_SLOTS; ++i) {
        HitRadioSlot empty = {0, 0, -1, 0};
        model->slots[i] = empty;
    }
}

static inline int hit_radio_model_show(HitRadioModel* model, int localCount, int slot, int character) {
    HitRadioSlot* event;
    if (localCount < 1 || localCount > HIT_RADIO_SLOTS || slot < 0 || slot >= localCount ||
        character < 0 || character >= 8) return 0;
    event = &model->slots[slot];
    if (event->cooldown > 0) return 0;
    event->age = 0;
    event->cooldown = HIT_RADIO_COOLDOWN;
    event->character = character;
    event->active = 1;
    return 1;
}

static inline void hit_radio_model_tick(HitRadioModel* model, float delta) {
    int i;
    if (!(delta > 0 && delta <= FLT_MAX)) return;
    for (i = 0; i < HIT_RADIO_SLOTS; ++i) {
        HitRadioSlot* event = &model->slots[i];
        event->cooldown = fmaxf(0, event->cooldown - delta);
        if (!event->active) continue;
        event->age = fminf(HIT_RADIO_DURATION, event->age + delta);
        if (event->age >= HIT_RADIO_DURATION) event->active = 0;
    }
}

static inline float hit_radio_model_reveal(const HitRadioSlot* event) {
    float amount;
    if (!event->active) return 0;
    amount = fminf(event->age / HIT_RADIO_SLIDE, (HIT_RADIO_DURATION - event->age) / HIT_RADIO_SLIDE);
    amount = fmaxf(0, fminf(1, amount));
    return amount * amount * (3 - 2 * amount);
}

/* Rectangles are half-open. Clip in HUD space, including signed wide-PC x,
 * without changing the renderer's global scissor or projection. */
static inline HudRect hit_radio_clip(HudRect rect, HudRect clip) {
    float x = fmaxf(rect.x, clip.x), y = fmaxf(rect.y, clip.y);
    return hud_layout_rect(x, y, fmaxf(0, fminf(rect.x + rect.w, clip.x + clip.w) - x),
                           fmaxf(0, fminf(rect.y + rect.h, clip.y + clip.h) - y));
}

static inline HitRadioLayout hit_radio_model_layout(HudRaceLayout hud, float reveal, int phone, float aspect) {
    HitRadioLayout result;
    HudRect view = hud.viewport;
    float edge = 4;
    float floorY = view.y + view.h - edge;
    float scale = fminf(view.w / 320, view.h / 240);
    if (phone) {
        float unit = fminf(240.0f / 7.5f, (240 * aspect) / 14);
        float gateTop = 240 - 2.65f * unit;
        float gateBottom = 240 - 1.05f * unit;
        scale = fminf(scale, 28.0f / 48);
        // Both touch layouts move the gate horizontally, not vertically.
        // Bottom-row panels fit below it with a two-unit gap; upper rows
        // remain at their own viewport bottom, above the control band.
        if (floorY > gateTop) scale = fminf(scale, (floorY - gateBottom - 2) / 48);
    }
    floorY = fmaxf(view.y, floorY);
    result.clip = hud_layout_rect(view.x, view.y, view.w, floorY - view.y);
    scale = fmaxf(0, fminf(scale, result.clip.h / 52));
    result.scale = scale;
    reveal = fmaxf(0, fminf(1, reveal));
    result.panel = hud_layout_rect(view.x + (view.w - 72 * scale) * .5f,
                                   floorY - 48 * scale * reveal, 72 * scale, 48 * scale);
    return result;
}
#endif
