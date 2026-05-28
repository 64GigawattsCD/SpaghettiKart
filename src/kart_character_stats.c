#include "kart_character_stats.h"

#include <defines.h>

#include "data/kart_character_stats_generated.inc.c"

const KartCharacterStats* kart_character_stats_get(s32 characterId) {
    if ((characterId < 0) || (characterId >= KART_CHARACTER_STATS_COUNT)) {
        characterId = MARIO;
    }

    return &gKartCharacterStats[characterId];
}

f32 kart_character_stats_get_steering_spring_multiplier(s32 characterId) {
    return kart_character_stats_get(characterId)->steeringSpringMultiplier;
}

f32 kart_character_stats_get_torque_multiplier(s32 characterId) {
    return kart_character_stats_get(characterId)->torqueMultiplier;
}
