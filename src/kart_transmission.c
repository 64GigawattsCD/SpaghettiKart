#include "kart_transmission.h"

#include <limits.h>
#include <stdbool.h>
#include <defines.h>
#include "kart_input.h"

#define KART_TRANSMISSION_PLAYER_COUNT 8

static s8 sKartGear[KART_TRANSMISSION_PLAYER_COUNT];
static bool sKartTransmissionInitialized[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartShiftBonusTimer[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartShiftPenaltyTimer[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartShiftLurchTimer[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartRpmFlareTimer[KART_TRANSMISSION_PLAYER_COUNT];
static bool sKartReverseWasEngaged[KART_TRANSMISSION_PLAYER_COUNT];
static f32 sKartClutchAmount[KART_TRANSMISSION_PLAYER_COUNT];
static f32 sKartLastThrottleAmount[KART_TRANSMISSION_PLAYER_COUNT];

static const f32 sGearMinSpeed[KART_GEAR_TOP + 1] = { 0.0f, 0.0f, 0.06f, 0.16f, 0.30f, 0.46f, 0.62f };
static const f32 sGearMaxSpeed[KART_GEAR_TOP + 1] = { 0.0f, 0.30f, 0.48f, 0.66f, 0.84f, 1.00f, 1.14f };
static const f32 sAutomaticUpshiftSpeed[KART_GEAR_TOP + 1] = { 0.0f, 0.25f, 0.43f, 0.61f, 0.79f, 0.96f, 1.20f };
static const f32 sAutomaticDownshiftSpeed[KART_GEAR_TOP + 1] = { 0.0f, 0.00f, 0.10f, 0.25f, 0.43f, 0.61f, 0.80f };

static s32 kart_transmission_clamp_player_index(s32 playerIndex) {
    if (playerIndex < 0) {
        return 0;
    }
    if (playerIndex >= KART_TRANSMISSION_PLAYER_COUNT) {
        return KART_TRANSMISSION_PLAYER_COUNT - 1;
    }
    return playerIndex;
}

static void kart_transmission_init_player(s32 playerIndex) {
    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    if (sKartTransmissionInitialized[playerIndex]) {
        return;
    }

    sKartGear[playerIndex] = KART_GEAR_FIRST;
    sKartTransmissionInitialized[playerIndex] = true;
}

bool kart_transmission_is_automatic(const Player* player) {
    if (player == NULL) {
        return false;
    }

    return ((player->type & PLAYER_CPU) == PLAYER_CPU) || ((player->type & PLAYER_HUMAN) != PLAYER_HUMAN);
}

static f32 kart_transmission_get_speed_ratio(const Player* player) {
    if ((player == NULL) || (player->topSpeed <= 0.0f)) {
        return 0.0f;
    }

    return player->currentSpeed / player->topSpeed;
}

static f32 kart_transmission_clampf(f32 value, f32 min, f32 max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static f32 kart_transmission_get_clutch_drive_ratio(const Player* player, s32 playerIndex) {
    f32 clutchAmount = sKartClutchAmount[playerIndex];
    f32 speedRatio = kart_transmission_get_speed_ratio(player);
    f32 biteRatio;

    if ((player != NULL) && (speedRatio < 0.055f) && (clutchAmount < 0.32f)) {
        clutchAmount *= 0.35f;
    }

    if (clutchAmount <= 0.35f) {
        return 1.0f;
    }
    if (clutchAmount >= 0.78f) {
        return 0.10f;
    }

    biteRatio = (clutchAmount - 0.35f) / 0.43f;
    return kart_transmission_clampf(1.0f - (biteRatio * biteRatio * 0.90f), 0.10f, 1.0f);
}

static f32 kart_transmission_get_gear_load(const Player* player, s32 gear) {
    f32 speedRatio;
    f32 gearRange;

    if (gear < KART_GEAR_FIRST) {
        return kart_transmission_get_speed_ratio(player);
    }

    speedRatio = kart_transmission_get_speed_ratio(player);
    gearRange = sGearMaxSpeed[gear] - sGearMinSpeed[gear];
    if (gearRange <= 0.0f) {
        return 0.0f;
    }

    return kart_transmission_clampf((speedRatio - sGearMinSpeed[gear]) / gearRange, 0.0f, 1.0f);
}

static void kart_transmission_update_automatic_gear(const Player* player, s32 playerIndex) {
    f32 speedRatio;
    s32 gear;

    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);

    if (!kart_transmission_is_automatic(player)) {
        return;
    }

    speedRatio = kart_transmission_get_speed_ratio(player);
    gear = sKartGear[playerIndex];
    if (gear < KART_GEAR_FIRST) {
        gear = KART_GEAR_FIRST;
    }

    while ((gear < KART_GEAR_TOP) && (speedRatio >= sAutomaticUpshiftSpeed[gear])) {
        gear++;
    }
    while ((gear > KART_GEAR_FIRST) && (speedRatio < sAutomaticDownshiftSpeed[gear])) {
        gear--;
    }

    sKartGear[playerIndex] = gear;
    sKartClutchAmount[playerIndex] = 0.0f;
    sKartShiftBonusTimer[playerIndex] = 0;
    sKartShiftPenaltyTimer[playerIndex] = 0;
    sKartShiftLurchTimer[playerIndex] = 0;
    sKartRpmFlareTimer[playerIndex] = 0;
}

static s32 kart_transmission_get_requested_gear(const struct Controller* controller) {
    if (kart_input_was_command_pressed(controller, KART_INPUT_SHIFT_NEUTRAL)) {
        return KART_GEAR_NEUTRAL;
    }
    if (kart_input_was_command_pressed(controller, KART_INPUT_SHIFT_REVERSE)) {
        return KART_GEAR_REVERSE;
    }
    if (kart_input_was_command_pressed(controller, KART_INPUT_SHIFT_GEAR_1)) {
        return 1;
    }
    if (kart_input_was_command_pressed(controller, KART_INPUT_SHIFT_GEAR_2)) {
        return 2;
    }
    if (kart_input_was_command_pressed(controller, KART_INPUT_SHIFT_GEAR_3)) {
        return 3;
    }
    if (kart_input_was_command_pressed(controller, KART_INPUT_SHIFT_GEAR_4)) {
        return 4;
    }
    if (kart_input_was_command_pressed(controller, KART_INPUT_SHIFT_GEAR_5)) {
        return 5;
    }
    if (kart_input_was_command_pressed(controller, KART_INPUT_SHIFT_GEAR_6)) {
        return 6;
    }
    return INT_MAX;
}

void kart_transmission_update(Player* player, const struct Controller* controller, s32 playerIndex) {
    s32 requestedGear;
    f32 clutchAmount;

    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);

    if (controller == NULL) {
        return;
    }

    clutchAmount = kart_input_get_command_value(controller, KART_INPUT_CLUTCH);
    sKartClutchAmount[playerIndex] = clutchAmount;

    requestedGear = kart_transmission_get_requested_gear(controller);
    if ((requestedGear != INT_MAX) && (requestedGear != sKartGear[playerIndex])) {
        if ((clutchAmount >= 0.35f) || (sKartGear[playerIndex] == KART_GEAR_NEUTRAL) ||
            (requestedGear == KART_GEAR_NEUTRAL)) {
            sKartShiftBonusTimer[playerIndex] = (clutchAmount >= 0.55f) ? 24 : 12;
            sKartShiftPenaltyTimer[playerIndex] = 0;
            sKartShiftLurchTimer[playerIndex] = 0;
        } else {
            sKartShiftBonusTimer[playerIndex] = 0;
            sKartShiftPenaltyTimer[playerIndex] = 18;
            sKartShiftLurchTimer[playerIndex] = 10;
            sKartRpmFlareTimer[playerIndex] = 18;
        }
        sKartGear[playerIndex] = requestedGear;
    }

    if (sKartShiftBonusTimer[playerIndex] > 0) {
        sKartShiftBonusTimer[playerIndex]--;
    }
    if (sKartShiftPenaltyTimer[playerIndex] > 0) {
        sKartShiftPenaltyTimer[playerIndex]--;
    }
    if (sKartShiftLurchTimer[playerIndex] > 0) {
        sKartShiftLurchTimer[playerIndex]--;
    }
    if (sKartRpmFlareTimer[playerIndex] > 0) {
        sKartRpmFlareTimer[playerIndex]--;
    }

    if (player != NULL) {
        if (sKartGear[playerIndex] == KART_GEAR_REVERSE) {
            player->kartProps |= BACK_UP;
            sKartReverseWasEngaged[playerIndex] = true;
        } else if (sKartReverseWasEngaged[playerIndex]) {
            player->kartProps &= ~BACK_UP;
            sKartReverseWasEngaged[playerIndex] = false;
        }
    }
}

f32 kart_transmission_get_drive_amount(const Player* player, s32 playerIndex, f32 throttleAmount) {
    f32 driveAmount;
    f32 speedRatio;
    f32 gearLoad;
    s32 gear;

    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);

    if ((player == NULL) || (throttleAmount <= 0.0f)) {
        return 0.0f;
    }

    kart_transmission_update_automatic_gear(player, playerIndex);
    sKartLastThrottleAmount[playerIndex] = throttleAmount;

    gear = sKartGear[playerIndex];
    if (gear == KART_GEAR_NEUTRAL) {
        return 0.0f;
    }

    driveAmount = throttleAmount * kart_transmission_get_clutch_drive_ratio(player, playerIndex);
    if (gear == KART_GEAR_REVERSE) {
        return driveAmount * 0.45f;
    }

    speedRatio = kart_transmission_get_speed_ratio(player);
    gearLoad = kart_transmission_get_gear_load(player, gear);

    if (speedRatio < sGearMinSpeed[gear]) {
        driveAmount *= 0.72f + (sKartClutchAmount[playerIndex] * 0.16f);
    } else if (speedRatio > sGearMaxSpeed[gear]) {
        driveAmount *= 0.42f;
    } else if (gearLoad > 0.88f) {
        driveAmount *= 1.0f - ((gearLoad - 0.88f) * 3.0f);
    }

    if (sKartShiftBonusTimer[playerIndex] > 0) {
        driveAmount *= 1.04f;
    }
    if (sKartShiftPenaltyTimer[playerIndex] > 0) {
        driveAmount *= 0.84f;
    }
    if (sKartShiftLurchTimer[playerIndex] > 0) {
        driveAmount *= 0.70f;
    }

    if (driveAmount > 1.15f) {
        driveAmount = 1.15f;
    }
    return driveAmount;
}

void kart_transmission_apply_speed_limits(Player* player, s32 playerIndex) {
    f32 maxGearSpeed;
    s32 gear;

    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);

    if ((player == NULL) || (player->topSpeed <= 0.0f)) {
        return;
    }

    kart_transmission_update_automatic_gear(player, playerIndex);

    gear = sKartGear[playerIndex];
    if (gear == KART_GEAR_REVERSE) {
        maxGearSpeed = player->topSpeed * 0.14f;
    } else if (gear > KART_GEAR_NEUTRAL) {
        maxGearSpeed = player->topSpeed * (sGearMaxSpeed[gear] + 0.025f);
    } else {
        return;
    }

    if (player->currentSpeed > maxGearSpeed) {
        player->currentSpeed -= (player->currentSpeed - maxGearSpeed) * 0.14f;
    }
}

s32 kart_transmission_get_gear(s32 playerIndex) {
    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);
    return sKartGear[playerIndex];
}

f32 kart_transmission_get_clutch_amount(s32 playerIndex) {
    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);
    return sKartClutchAmount[playerIndex];
}

f32 kart_transmission_get_engine_rpm(const Player* player, s32 playerIndex) {
    f32 gearLoad;
    f32 clutchAmount;
    f32 rpm;
    s32 gear;

    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);

    if (player == NULL) {
        return 900.0f;
    }

    gear = sKartGear[playerIndex];
    if (gear < KART_GEAR_FIRST) {
        return 900.0f + (kart_transmission_get_speed_ratio(player) * 1200.0f) +
               (sKartLastThrottleAmount[playerIndex] * 1200.0f);
    }

    gearLoad = kart_transmission_get_gear_load(player, gear);
    clutchAmount = sKartClutchAmount[playerIndex];
    rpm = 900.0f + (gearLoad * 6200.0f) + (sKartLastThrottleAmount[playerIndex] * 700.0f);
    if (clutchAmount > 0.55f) {
        rpm += (clutchAmount - 0.55f) * 2600.0f * sKartLastThrottleAmount[playerIndex];
    }
    if (sKartRpmFlareTimer[playerIndex] > 0) {
        rpm += ((f32) sKartRpmFlareTimer[playerIndex] / 18.0f) * 1800.0f;
    }
    if (rpm > 8200.0f) {
        rpm = 8200.0f;
    }

    return rpm;
}
