#ifndef HUD_RACE_LAYOUT_H
#define HUD_RACE_LAYOUT_H
#include "hud_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HudRaceLayout {
    HudRect viewport;
    HudRect column;
    HudRect laps;
    HudRect mapSlot;
    HudRect map;
    HudRect rpm;
    f32 scale;
    f32 mapScale;
} HudRaceLayout;

// Global HUD coordinates: 240 units high, centered on x=160 at any aspect.
HudRect hud_race_viewport(f32 aspect, s32 screenMode, s32 player);
HudRaceLayout hud_race_layout(HudRect viewport, f32 marginScale, HudVec2 mapSize, bool laps, bool rpm);
void get_race_hud_layout(s32 playerId, HudRaceLayout* result);

#ifdef __cplusplus
}
#endif
#endif
