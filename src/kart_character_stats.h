#ifndef KART_CHARACTER_STATS_H
#define KART_CHARACTER_STATS_H

#include <common_structs.h>

#define KART_CHARACTER_STATS_COUNT 8
#define KART_CHARACTER_ACCEL_COUNT 10
#define KART_CC_MODE_COUNT 5

typedef struct {
    s32 characterId;
    const char* characterName;
    const char* weightClass;
    f32 steeringSpringMultiplier;
    f32 torqueMultiplier;
    f32 topSpeedByMode[KART_CC_MODE_COUNT];
    f32 kartTopSpeed;
    f32 boundingBoxSize;
    f32 handling;
    f32 turnSpeedReduction0;
    f32 turnSpeedReduction1;
    f32 tripleABoost;
    f32 acceleration[KART_CHARACTER_ACCEL_COUNT];
} KartCharacterStats;

extern const KartCharacterStats gKartCharacterStats[KART_CHARACTER_STATS_COUNT];

const KartCharacterStats* kart_character_stats_get(s32 characterId);
f32 kart_character_stats_get_steering_spring_multiplier(s32 characterId);
f32 kart_character_stats_get_torque_multiplier(s32 characterId);

#endif
