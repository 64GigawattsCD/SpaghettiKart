#ifndef KART_TRANSMISSION_H
#define KART_TRANSMISSION_H

#include <common_structs.h>

#define KART_GEAR_REVERSE -1
#define KART_GEAR_NEUTRAL 0
#define KART_GEAR_FIRST 1
#define KART_GEAR_TOP 6

typedef enum {
    KART_SHIFT_FEEDBACK_NONE,
    KART_SHIFT_FEEDBACK_GOOD,
    KART_SHIFT_FEEDBACK_BAD,
} KartShiftFeedback;

void kart_transmission_update(Player* player, const struct Controller* controller, s32 playerIndex);
f32 kart_transmission_get_drive_amount(const Player* player, s32 playerIndex, f32 throttleAmount);
void kart_transmission_apply_speed_limits(Player* player, s32 playerIndex);
bool kart_transmission_is_automatic(const Player* player);
bool kart_transmission_is_reverse_gear(s32 playerIndex);
s32 kart_transmission_get_gear(s32 playerIndex);
f32 kart_transmission_get_clutch_amount(s32 playerIndex);
f32 kart_transmission_get_engine_rpm(const Player* player, s32 playerIndex);
f32 kart_transmission_get_shift_ideal_rpm_min(const Player* player);
f32 kart_transmission_get_shift_ideal_rpm_max(const Player* player);
f32 kart_transmission_get_shift_over_rpm(const Player* player);
KartShiftFeedback kart_transmission_get_shift_feedback(s32 playerIndex);
s16 kart_transmission_get_shift_feedback_timer(s32 playerIndex);

#endif
