#include "kart_transmission.h"

#include <libultraship.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <defines.h>
#include "kart_input.h"
#include "kart_character_stats.h"
#include "code_80057C60.h"
#include "math_util.h"
#include "port/audio/HMAS.h"
#include "audio/external.h"
#include "sounds.h"

#define KART_TRANSMISSION_PLAYER_COUNT 8
#define KART_SHIFT_GRIND_SOUND_COUNT 3
#define KART_SHIFT_GRIND_SOUND_ID_BASE 7000
#define KART_SHIFT_BAD_SOUND SOUND_ACTION_TYRE_SQUEAL
#define KART_SHIFT_GOOD_SOUND SOUND_ACTION_PING
#define KART_SHIFT_GRIND_GRAPHIC_FRAMES 40
#define KART_SHIFT_IDEAL_RPM_MIN_DEFAULT 3800.0f
#define KART_SHIFT_IDEAL_RPM_MAX_DEFAULT 7200.0f
#define KART_SHIFT_OVER_RPM_DEFAULT 8000.0f
#define KART_SHIFT_SEQUENTIAL_STICK_DEADZONE 0.25f

static s8 sKartGear[KART_TRANSMISSION_PLAYER_COUNT];
static bool sKartTransmissionInitialized[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartShiftBonusTimer[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartShiftPenaltyTimer[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartShiftLurchTimer[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartRpmFlareTimer[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartShiftFeedbackTimer[KART_TRANSMISSION_PLAYER_COUNT];
static s16 sKartShiftPendingFrames[KART_TRANSMISSION_PLAYER_COUNT];
static f32 sKartClutchAmount[KART_TRANSMISSION_PLAYER_COUNT];
static f32 sKartPreviousClutchAmount[KART_TRANSMISSION_PLAYER_COUNT];
static f32 sKartLastThrottleAmount[KART_TRANSMISSION_PLAYER_COUNT];
static f32 sKartShiftClutchAtRequest[KART_TRANSMISSION_PLAYER_COUNT];
static f32 sKartShiftRpmAtRequest[KART_TRANSMISSION_PLAYER_COUNT];
static s8 sKartShiftPendingFromGear[KART_TRANSMISSION_PLAYER_COUNT];
static s8 sKartShiftPendingToGear[KART_TRANSMISSION_PLAYER_COUNT];
static bool sKartShiftPending[KART_TRANSMISSION_PLAYER_COUNT];
static bool sKartShiftSessionHadClutch[KART_TRANSMISSION_PLAYER_COUNT];
static s8 sKartSequentialShiftDirection[KART_TRANSMISSION_PLAYER_COUNT];
static KartShiftFeedback sKartShiftFeedback[KART_TRANSMISSION_PLAYER_COUNT];
static bool sKartShiftGrindSoundsLoaded;
static s8 sKartLastShiftGrindSoundIndex = -1;

static const f32 sGearMinSpeed[KART_GEAR_TOP + 1] = { 0.0f, 0.0f, 0.06f, 0.16f, 0.30f, 0.46f, 0.62f };
static const f32 sGearMaxSpeed[KART_GEAR_TOP + 1] = { 0.0f, 0.30f, 0.48f, 0.66f, 0.84f, 1.00f, 1.14f };
static const f32 sAutomaticUpshiftSpeed[KART_GEAR_TOP + 1] = { 0.0f, 0.25f, 0.43f, 0.61f, 0.79f, 0.96f, 1.20f };
static const f32 sAutomaticDownshiftSpeed[KART_GEAR_TOP + 1] = { 0.0f, 0.00f, 0.10f, 0.25f, 0.43f, 0.61f, 0.80f };
static const f32 sGearTorqueMultiplier[KART_GEAR_TOP + 1] = { 0.0f, 1.36f, 1.20f, 1.04f, 0.88f, 0.72f, 0.56f };
static const f32 sGearLaunchMultiplier[KART_GEAR_TOP + 1] = { 0.0f, 1.00f, 0.58f, 0.28f, 0.15f, 0.08f, 0.04f };

static bool kart_transmission_is_drive_gear(s32 gear) {
    return (gear >= KART_GEAR_FIRST) || (gear == KART_GEAR_REVERSE);
}

static s8 kart_transmission_get_requested_sequential_direction(const struct Controller* controller, s32 playerIndex) {
    s32 direction = 0;
    f32 rightStickAxis;
    bool upPressed;
    bool downPressed;

    if (controller == NULL) {
        return 0;
    }

    rightStickAxis = ((f32) controller->rightRawStickY) / 32767.0f;
    if (rightStickAxis > KART_SHIFT_SEQUENTIAL_STICK_DEADZONE) {
        direction = -1;
    } else if (rightStickAxis < -KART_SHIFT_SEQUENTIAL_STICK_DEADZONE) {
        direction = 1;
    } else {
        upPressed = kart_input_is_command_active(controller, KART_INPUT_SHIFT_GEAR_UP);
        downPressed = kart_input_is_command_active(controller, KART_INPUT_SHIFT_GEAR_DOWN);

        if (upPressed && !downPressed) {
            direction = 1;
        } else if (downPressed && !upPressed) {
            direction = -1;
        } else if (upPressed && downPressed) {
            direction = 0;
        }
    }

    if (direction == 0) {
        sKartSequentialShiftDirection[playerIndex] = 0;
        return 0;
    }

    if (sKartSequentialShiftDirection[playerIndex] == direction) {
        return 0;
    }

    sKartSequentialShiftDirection[playerIndex] = (s8) direction;
    return (s8) direction;
}

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
    sKartSequentialShiftDirection[playerIndex] = 0;
    sKartTransmissionInitialized[playerIndex] = true;
}

static void kart_transmission_clear_shift_session(s32 playerIndex) {
    sKartShiftPending[playerIndex] = false;
    sKartShiftSessionHadClutch[playerIndex] = false;
    sKartShiftPendingFrames[playerIndex] = 0;
    sKartShiftFeedback[playerIndex] = KART_SHIFT_FEEDBACK_NONE;
    sKartShiftFeedbackTimer[playerIndex] = 0;
    sKartShiftPenaltyTimer[playerIndex] = 0;
    sKartShiftLurchTimer[playerIndex] = 0;
    sKartRpmFlareTimer[playerIndex] = 0;
    sKartSequentialShiftDirection[playerIndex] = 0;
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

static f32 kart_transmission_get_cvarf(const char* key, f32 defaultValue) {
    return CVarGetFloat(key, defaultValue);
}

f32 kart_transmission_get_shift_ideal_rpm_min(const Player* player) {
    f32 defaultValue = KART_SHIFT_IDEAL_RPM_MIN_DEFAULT;

    if ((player != NULL) && (player->characterId >= 0) && (player->characterId < KART_CHARACTER_STATS_COUNT)) {
        defaultValue = kart_character_stats_get_shift_ideal_rpm_min(player->characterId);
    }
    return kart_transmission_get_cvarf("gArcadeKart.ShiftIdealRpmMin", defaultValue);
}

f32 kart_transmission_get_shift_ideal_rpm_max(const Player* player) {
    f32 defaultValue = KART_SHIFT_IDEAL_RPM_MAX_DEFAULT;

    if ((player != NULL) && (player->characterId >= 0) && (player->characterId < KART_CHARACTER_STATS_COUNT)) {
        defaultValue = kart_character_stats_get_shift_ideal_rpm_max(player->characterId);
    }
    return kart_transmission_get_cvarf("gArcadeKart.ShiftIdealRpmMax", defaultValue);
}

f32 kart_transmission_get_shift_over_rpm(const Player* player) {
    f32 defaultValue = KART_SHIFT_OVER_RPM_DEFAULT;

    if ((player != NULL) && (player->characterId >= 0) && (player->characterId < KART_CHARACTER_STATS_COUNT)) {
        defaultValue = kart_character_stats_get_shift_over_rpm(player->characterId);
    }
    return kart_transmission_get_cvarf("gArcadeKart.ShiftOverRpm", defaultValue);
}

static void kart_transmission_register_grind_sound(s32 index, const char* fileName) {
    char appDirectoryPath[128];
    char sourceDirectoryPath[160];
    s32 soundId = KART_SHIFT_GRIND_SOUND_ID_BASE + index;

    snprintf(appDirectoryPath, sizeof(appDirectoryPath), "arcadekart_audio/%s", fileName);
    HMAS_RegisterSoundFromFile(soundId, appDirectoryPath);
    if (HMAS_IsIDRegistered(soundId)) {
        return;
    }

    snprintf(sourceDirectoryPath, sizeof(sourceDirectoryPath), "assets/arcadekart_audio/%s", fileName);
    HMAS_RegisterSoundFromFile(soundId, sourceDirectoryPath);
}

static void kart_transmission_load_grind_sounds(void) {
    if (sKartShiftGrindSoundsLoaded) {
        return;
    }

    kart_transmission_register_grind_sound(0, "bad_shift_grind_a.ogg");
    kart_transmission_register_grind_sound(1, "bad_shift_grind_b.ogg");
    kart_transmission_register_grind_sound(2, "bad_shift_grind_c.ogg");
    sKartShiftGrindSoundsLoaded = true;
}

static s32 kart_transmission_pick_grind_sound_index(void) {
    s32 offset;

    if (sKartLastShiftGrindSoundIndex < 0) {
        return random_int(KART_SHIFT_GRIND_SOUND_COUNT);
    }

    offset = 1 + random_int(2);
    return (sKartLastShiftGrindSoundIndex + offset) % KART_SHIFT_GRIND_SOUND_COUNT;
}

static void kart_transmission_play_grind_sound(void) {
    s32 soundIndex;
    s32 soundId;

    kart_transmission_load_grind_sounds();
    soundIndex = kart_transmission_pick_grind_sound_index();
    soundId = KART_SHIFT_GRIND_SOUND_ID_BASE + soundIndex;
    sKartLastShiftGrindSoundIndex = soundIndex;

    if (HMAS_IsIDRegistered(soundId)) {
        HMAS_Play(HMAS_SFX, soundId, false);
    } else {
        play_sound2(KART_SHIFT_BAD_SOUND);
    }
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

static void kart_transmission_spawn_good_shift_sparks(Player* player) {
    s32 i;

    if (player == NULL) {
        return;
    }

    for (i = 0; i < 2; i++) {
        if (player->particlePool1[i].isAlive != 0) {
            continue;
        }
        if (i == 0) {
            set_particle_position_and_rotation(player, &player->particlePool1[i], player->tyres[BACK_LEFT].pos[0],
                                               player->tyres[BACK_LEFT].baseHeight, player->tyres[BACK_LEFT].pos[2],
                                               player->tyres[BACK_LEFT].surfaceType, 0);
        } else {
            set_particle_position_and_rotation(player, &player->particlePool1[i], player->tyres[BACK_RIGHT].pos[0],
                                               player->tyres[BACK_RIGHT].baseHeight, player->tyres[BACK_RIGHT].pos[2],
                                               player->tyres[BACK_RIGHT].surfaceType, 0);
        }
        init_particle_player(&player->particlePool1[i], POOL_1_PARTICLE_TYPE_8, 0.70f);
        set_particle_colour(&player->particlePool1[i], 0xFF9600, 0xFF);
    }
}

static void kart_transmission_apply_shift_feedback(Player* player, s32 playerIndex, KartShiftFeedback feedback) {
    f32 boostRatio;
    f32 badSpeedPenalty;

    sKartShiftFeedback[playerIndex] = feedback;
    sKartShiftFeedbackTimer[playerIndex] = KART_SHIFT_GRIND_GRAPHIC_FRAMES;

    if (feedback == KART_SHIFT_FEEDBACK_GOOD) {
        sKartShiftBonusTimer[playerIndex] = (s16) kart_transmission_get_cvarf("gArcadeKart.ShiftGoodBoostFrames", 30.0f);
        sKartShiftPenaltyTimer[playerIndex] = 0;
        sKartShiftLurchTimer[playerIndex] = 0;
        if (player != NULL) {
            boostRatio = kart_transmission_get_cvarf("gArcadeKart.ShiftGoodImmediateBoostRatio", 0.012f);
            player->currentSpeed += player->topSpeed * boostRatio;
            if (player->currentSpeed > player->topSpeed) {
                player->currentSpeed = player->topSpeed;
            }
            kart_transmission_spawn_good_shift_sparks(player);
        }
        play_sound2(KART_SHIFT_GOOD_SOUND);
        return;
    }

    if (feedback == KART_SHIFT_FEEDBACK_BAD) {
        sKartShiftBonusTimer[playerIndex] = 0;
        sKartShiftPenaltyTimer[playerIndex] = (s16) kart_transmission_get_cvarf("gArcadeKart.ShiftBadPenaltyFrames", 36.0f);
        sKartShiftLurchTimer[playerIndex] = 14;
        sKartRpmFlareTimer[playerIndex] = 20;
        if (player != NULL) {
            badSpeedPenalty = kart_transmission_get_cvarf("gArcadeKart.ShiftBadSpeedPenalty", 0.88f);
            player->currentSpeed *= badSpeedPenalty;
        }
        kart_transmission_play_grind_sound();
    }
}

static const char* kart_transmission_get_gear_label(s32 gear) {
    switch (gear) {
        case KART_GEAR_REVERSE:
            return "R";
        case KART_GEAR_NEUTRAL:
            return "N";
        case KART_GEAR_FIRST:
            return "1";
        case 2:
            return "2";
        case 3:
            return "3";
        case 4:
            return "4";
        case 5:
            return "5";
        case KART_GEAR_TOP:
            return "6";
        default:
            return "?";
    }
}

static void kart_transmission_log_shift_event(const char* eventName, s32 playerIndex, s32 fromGear, s32 toGear,
                                              f32 clutchAmount, f32 rpmAtRequest, f32 rpmNow, s32 frames,
                                              bool hadClutch) {
    FILE* logFile = fopen("logs/ArcadeKartShift.log", "a");

    if (logFile == NULL) {
        logFile = fopen("ArcadeKartShift.log", "a");
        if (logFile == NULL) {
            return;
        }
    }

    fprintf(logFile,
            "[ArcadeKart Shift] %s P%d %s->%s clutch %.3f rpmStart %.0f rpmNow %.0f frames %d hadClutch %d\n",
            eventName, playerIndex, kart_transmission_get_gear_label(fromGear),
            kart_transmission_get_gear_label(toGear), clutchAmount, rpmAtRequest, rpmNow, frames, hadClutch ? 1 : 0);
    fclose(logFile);
}

static void kart_transmission_score_pending_shift(Player* player, s32 playerIndex, bool clutchReleased) {
    bool isUpshift;
    bool isDownshift;
    bool clutchWasGood;
    bool clutchWasAdequate;
    bool releasedQuickly;
    bool rpmWasIdeal;
    bool rpmWasHigh;
    f32 idealRpmMin;
    f32 idealRpmMax;
    s32 holdLimitFrames;

    if (!sKartShiftPending[playerIndex]) {
        return;
    }

    holdLimitFrames = (s32) kart_transmission_get_cvarf("gArcadeKart.ShiftGoodReleaseFrames", 45.0f);
    if (!clutchReleased && sKartShiftSessionHadClutch[playerIndex] &&
        (sKartShiftPendingFrames[playerIndex] <= holdLimitFrames)) {
        return;
    }

    isUpshift = (sKartShiftPendingFromGear[playerIndex] >= KART_GEAR_FIRST) &&
                (sKartShiftPendingToGear[playerIndex] > sKartShiftPendingFromGear[playerIndex]);
    isDownshift = (sKartShiftPendingFromGear[playerIndex] > KART_GEAR_FIRST) &&
                  (sKartShiftPendingToGear[playerIndex] >= KART_GEAR_FIRST) &&
                  (sKartShiftPendingToGear[playerIndex] < sKartShiftPendingFromGear[playerIndex]);
    clutchWasGood = sKartShiftClutchAtRequest[playerIndex] >=
                    kart_transmission_get_cvarf("gArcadeKart.ShiftGoodClutchThreshold", 0.84f);
    clutchWasAdequate = sKartShiftClutchAtRequest[playerIndex] >=
                        kart_transmission_get_cvarf("gArcadeKart.ShiftAdequateClutchThreshold", 0.45f);
    releasedQuickly = sKartShiftPendingFrames[playerIndex] <= holdLimitFrames;
    idealRpmMin = kart_transmission_get_shift_ideal_rpm_min(player);
    idealRpmMax = kart_transmission_get_shift_ideal_rpm_max(player);
    rpmWasIdeal = (sKartShiftRpmAtRequest[playerIndex] >= idealRpmMin) &&
                  (sKartShiftRpmAtRequest[playerIndex] <= idealRpmMax);
    rpmWasHigh = sKartShiftRpmAtRequest[playerIndex] > idealRpmMax;

    CVarSetInteger("gArcadeKart.DebugShiftFromGear", sKartShiftPendingFromGear[playerIndex]);
    CVarSetInteger("gArcadeKart.DebugShiftToGear", sKartShiftPendingToGear[playerIndex]);
    CVarSetFloat("gArcadeKart.DebugShiftClutchAtRequest", sKartShiftClutchAtRequest[playerIndex]);
    CVarSetFloat("gArcadeKart.DebugShiftRpmAtRequest", sKartShiftRpmAtRequest[playerIndex]);
    CVarSetInteger("gArcadeKart.DebugShiftPendingFrames", sKartShiftPendingFrames[playerIndex]);
    CVarSetInteger("gArcadeKart.DebugShiftHadClutch", sKartShiftSessionHadClutch[playerIndex] ? 1 : 0);

    kart_transmission_log_shift_event("SCORE", playerIndex, sKartShiftPendingFromGear[playerIndex],
                                      sKartShiftPendingToGear[playerIndex], sKartShiftClutchAtRequest[playerIndex],
                                      sKartShiftRpmAtRequest[playerIndex],
                                      kart_transmission_get_engine_rpm(player, playerIndex),
                                      sKartShiftPendingFrames[playerIndex], sKartShiftSessionHadClutch[playerIndex]);
    printf("[ArcadeKart Shift] P%d %s->%s clutch %.2f rpm %.0f frames %d up %d down %d goodClutch %d adequateClutch %d quick %d idealRpm %d highRpm %d\n",
           playerIndex, kart_transmission_get_gear_label(sKartShiftPendingFromGear[playerIndex]),
           kart_transmission_get_gear_label(sKartShiftPendingToGear[playerIndex]),
           sKartShiftClutchAtRequest[playerIndex], sKartShiftRpmAtRequest[playerIndex],
           sKartShiftPendingFrames[playerIndex], isUpshift, isDownshift, clutchWasGood, clutchWasAdequate,
           releasedQuickly, rpmWasIdeal, rpmWasHigh);

    if (!kart_transmission_is_drive_gear(sKartShiftPendingToGear[playerIndex]) ||
        (sKartShiftPendingFromGear[playerIndex] == sKartShiftPendingToGear[playerIndex])) {
        sKartShiftFeedback[playerIndex] = KART_SHIFT_FEEDBACK_NONE;
        sKartShiftFeedbackTimer[playerIndex] = 0;
    } else if (isUpshift && clutchWasGood && releasedQuickly && rpmWasIdeal) {
        kart_transmission_apply_shift_feedback(player, playerIndex, KART_SHIFT_FEEDBACK_GOOD);
    } else if (isUpshift && clutchWasGood && releasedQuickly && rpmWasHigh) {
        sKartShiftFeedback[playerIndex] = KART_SHIFT_FEEDBACK_NONE;
        sKartShiftFeedbackTimer[playerIndex] = 0;
    } else if (clutchWasAdequate && (isDownshift || (isUpshift && !releasedQuickly))) {
        sKartShiftFeedback[playerIndex] = KART_SHIFT_FEEDBACK_NONE;
        sKartShiftFeedbackTimer[playerIndex] = 0;
    } else {
        kart_transmission_apply_shift_feedback(player, playerIndex, KART_SHIFT_FEEDBACK_BAD);
    }

    sKartShiftPending[playerIndex] = false;
    sKartShiftSessionHadClutch[playerIndex] = false;
    sKartShiftPendingFrames[playerIndex] = 0;
}

static void kart_transmission_begin_shift_session(Player* player, s32 playerIndex, s32 fromGear, f32 clutchAmount,
                                                  bool hadClutch) {
    if (!kart_transmission_is_drive_gear(fromGear)) {
        return;
    }

    sKartShiftPending[playerIndex] = true;
    sKartShiftPendingFrames[playerIndex] = 0;
    sKartShiftPendingFromGear[playerIndex] = fromGear;
    sKartShiftPendingToGear[playerIndex] = fromGear;
    sKartShiftClutchAtRequest[playerIndex] = clutchAmount;
    sKartShiftRpmAtRequest[playerIndex] = kart_transmission_get_engine_rpm(player, playerIndex);
    sKartShiftSessionHadClutch[playerIndex] = hadClutch;
    kart_transmission_log_shift_event("BEGIN", playerIndex, fromGear, fromGear, clutchAmount,
                                      sKartShiftRpmAtRequest[playerIndex], sKartShiftRpmAtRequest[playerIndex], 0,
                                      hadClutch);
}

static void kart_transmission_update_shift_session(Player* player, s32 playerIndex, s32 requestedGear,
                                                   f32 clutchAmount) {
    if (!sKartShiftPending[playerIndex]) {
        return;
    }

    if (clutchAmount > sKartShiftClutchAtRequest[playerIndex]) {
        sKartShiftClutchAtRequest[playerIndex] = clutchAmount;
    }
    if (kart_transmission_is_drive_gear(requestedGear)) {
        if (requestedGear != sKartShiftPendingToGear[playerIndex]) {
            sKartShiftPendingToGear[playerIndex] = requestedGear;
            kart_transmission_log_shift_event("TARGET", playerIndex, sKartShiftPendingFromGear[playerIndex],
                                              requestedGear, sKartShiftClutchAtRequest[playerIndex],
                                              sKartShiftRpmAtRequest[playerIndex],
                                              kart_transmission_get_engine_rpm(player, playerIndex),
                                              sKartShiftPendingFrames[playerIndex],
                                              sKartShiftSessionHadClutch[playerIndex]);
        } else {
            sKartShiftPendingToGear[playerIndex] = requestedGear;
        }
    }
    if (!sKartShiftSessionHadClutch[playerIndex] &&
        (sKartShiftClutchAtRequest[playerIndex] <
         kart_transmission_get_cvarf("gArcadeKart.ShiftAdequateClutchThreshold", 0.45f)) &&
        (sKartShiftPendingToGear[playerIndex] != sKartShiftPendingFromGear[playerIndex])) {
        kart_transmission_score_pending_shift(player, playerIndex, false);
    }
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

static f32 kart_transmission_get_weight_torque_multiplier(const Player* player) {
    if ((player == NULL) || (player->characterId >= 8)) {
        return 1.0f;
    }

    return kart_character_stats_get_torque_multiplier(player->characterId);
}

static f32 kart_transmission_get_launch_torque_multiplier(f32 speedRatio, s32 gear) {
    f32 launchBlend;

    if (gear <= KART_GEAR_FIRST) {
        return 1.0f;
    }

    launchBlend = kart_transmission_clampf(speedRatio / 0.08f, 0.0f, 1.0f);
    return sGearLaunchMultiplier[gear] + ((1.0f - sGearLaunchMultiplier[gear]) * launchBlend);
}

static f32 kart_transmission_get_underspeed_bog_multiplier(f32 speedRatio, s32 gear) {
    f32 underspeedRatio;
    f32 bogAmount;

    if ((gear <= KART_GEAR_FIRST) || (speedRatio >= sGearMinSpeed[gear])) {
        return 1.0f;
    }

    underspeedRatio = (sGearMinSpeed[gear] - speedRatio) / (sGearMinSpeed[gear] + 0.08f);
    bogAmount = underspeedRatio * (0.42f + (gear * 0.08f));
    return kart_transmission_clampf(1.0f - bogAmount, 0.12f, 1.0f);
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

static s32 kart_transmission_get_sequential_gear(s32 currentGear, s32 direction) {
    if (direction > 0) {
        if (currentGear == KART_GEAR_REVERSE) {
            return KART_GEAR_NEUTRAL;
        }
        if (currentGear == KART_GEAR_NEUTRAL) {
            return KART_GEAR_FIRST;
        }
        if (currentGear < KART_GEAR_TOP) {
            return currentGear + 1;
        }
        return KART_GEAR_TOP;
    }

    if (currentGear > KART_GEAR_FIRST) {
        return currentGear - 1;
    }
    if (currentGear == KART_GEAR_FIRST) {
        return KART_GEAR_NEUTRAL;
    }
    return KART_GEAR_REVERSE;
}

static s32 kart_transmission_get_requested_gear(const struct Controller* controller, s32 playerIndex, s32 currentGear,
                                                f32 clutchAmount) {
    f32 sequentialClutchThreshold;
    s32 sequentialDirection;

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
    sequentialClutchThreshold = kart_transmission_get_cvarf("gArcadeKart.ShiftSequentialClutchThreshold",
                                                            kart_transmission_get_cvarf(
                                                                "gArcadeKart.ShiftClutchPressThreshold", 0.35f));
    if (clutchAmount < sequentialClutchThreshold) {
        return INT_MAX;
    }
    sequentialDirection = kart_transmission_get_requested_sequential_direction(controller, playerIndex);
    if (sequentialDirection > 0) {
        return kart_transmission_get_sequential_gear(currentGear, 1);
    }
    if (sequentialDirection < 0) {
        return kart_transmission_get_sequential_gear(currentGear, -1);
    }
    return INT_MAX;
}

void kart_transmission_update(Player* player, const struct Controller* controller, s32 playerIndex) {
    s32 requestedGear;
    f32 clutchAmount;
    bool clutchReleased;
    bool clutchPressed;

    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);

    if (controller == NULL) {
        return;
    }

    clutchAmount = kart_input_get_command_value(controller, KART_INPUT_CLUTCH);
    clutchReleased = (sKartPreviousClutchAmount[playerIndex] >
                      kart_transmission_get_cvarf("gArcadeKart.ShiftClutchReleaseThreshold", 0.25f)) &&
                     (clutchAmount <= kart_transmission_get_cvarf("gArcadeKart.ShiftClutchReleaseThreshold", 0.25f));
    clutchPressed = (sKartPreviousClutchAmount[playerIndex] <=
                     kart_transmission_get_cvarf("gArcadeKart.ShiftClutchPressThreshold", 0.35f)) &&
                    (clutchAmount > kart_transmission_get_cvarf("gArcadeKart.ShiftClutchPressThreshold", 0.35f));
    sKartClutchAmount[playerIndex] = clutchAmount;

    requestedGear = kart_transmission_get_requested_gear(controller, playerIndex, sKartGear[playerIndex], clutchAmount);
    if ((player != NULL) && ((player->type & PLAYER_START_SEQUENCE) == PLAYER_START_SEQUENCE)) {
        if ((requestedGear != INT_MAX) && (requestedGear != KART_GEAR_NEUTRAL)) {
            sKartGear[playerIndex] = requestedGear;
        }
        kart_transmission_clear_shift_session(playerIndex);
        sKartPreviousClutchAmount[playerIndex] = clutchAmount;
        return;
    }

    if (clutchPressed && !sKartShiftPending[playerIndex]) {
        kart_transmission_begin_shift_session(player, playerIndex, sKartGear[playerIndex], clutchAmount, true);
    }
    if (sKartShiftPending[playerIndex]) {
        sKartShiftPendingFrames[playerIndex]++;
        if (clutchAmount > sKartShiftClutchAtRequest[playerIndex]) {
            sKartShiftClutchAtRequest[playerIndex] = clutchAmount;
        }
    }

    if ((requestedGear != INT_MAX) && (requestedGear != sKartGear[playerIndex])) {
        if (!sKartShiftPending[playerIndex]) {
            kart_transmission_begin_shift_session(player, playerIndex, sKartGear[playerIndex], clutchAmount,
                                                 clutchAmount > kart_transmission_get_cvarf(
                                                                    "gArcadeKart.ShiftClutchPressThreshold", 0.35f));
        }
        kart_transmission_update_shift_session(player, playerIndex, requestedGear, clutchAmount);
        if ((clutchAmount >= 0.35f) || (sKartGear[playerIndex] == KART_GEAR_NEUTRAL) ||
            (requestedGear == KART_GEAR_NEUTRAL)) {
            if (requestedGear == KART_GEAR_NEUTRAL) {
                sKartShiftPenaltyTimer[playerIndex] = 0;
                sKartShiftLurchTimer[playerIndex] = 0;
            }
        } else {
            sKartShiftBonusTimer[playerIndex] = 0;
            sKartShiftPenaltyTimer[playerIndex] = 18;
            sKartShiftLurchTimer[playerIndex] = 10;
            sKartRpmFlareTimer[playerIndex] = 18;
        }
        sKartGear[playerIndex] = requestedGear;
    }
    if (sKartShiftPending[playerIndex]) {
        kart_transmission_update_shift_session(player, playerIndex, sKartGear[playerIndex], clutchAmount);
    }
    kart_transmission_score_pending_shift(player, playerIndex, clutchReleased);

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
    if (sKartShiftFeedbackTimer[playerIndex] > 0) {
        sKartShiftFeedbackTimer[playerIndex]--;
    } else {
        sKartShiftFeedback[playerIndex] = KART_SHIFT_FEEDBACK_NONE;
    }
    sKartPreviousClutchAmount[playerIndex] = clutchAmount;

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
        return driveAmount * kart_transmission_get_cvarf("gArcadeKart.ReverseDriveMultiplier", 0.75f);
    }

    speedRatio = kart_transmission_get_speed_ratio(player);
    gearLoad = kart_transmission_get_gear_load(player, gear);
    driveAmount *= sGearTorqueMultiplier[gear] * kart_transmission_get_weight_torque_multiplier(player);
    driveAmount *= kart_transmission_get_launch_torque_multiplier(speedRatio, gear);
    driveAmount *= kart_transmission_get_underspeed_bog_multiplier(speedRatio, gear);

    if (speedRatio < sGearMinSpeed[gear]) {
        driveAmount *= 0.82f + (sKartClutchAmount[playerIndex] * 0.12f);
    } else if (speedRatio > sGearMaxSpeed[gear]) {
        driveAmount *= 0.30f;
    } else if (gearLoad > 0.88f) {
        driveAmount *= 1.0f - ((gearLoad - 0.88f) * 4.0f);
    }

    if (sKartShiftBonusTimer[playerIndex] > 0) {
        driveAmount *= kart_transmission_get_cvarf("gArcadeKart.ShiftGoodDriveMultiplier", 1.14f);
    }
    if (sKartShiftPenaltyTimer[playerIndex] > 0) {
        driveAmount *= kart_transmission_get_cvarf("gArcadeKart.ShiftBadDriveMultiplier", 0.70f);
    }
    if (sKartShiftLurchTimer[playerIndex] > 0) {
        driveAmount *= 0.70f;
    }

    if (driveAmount > 1.38f) {
        driveAmount = 1.38f;
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
        maxGearSpeed = player->topSpeed * kart_transmission_get_cvarf("gArcadeKart.ReverseSpeedRatio", 0.44f);
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

bool kart_transmission_is_reverse_gear(s32 playerIndex) {
    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);
    return sKartGear[playerIndex] == KART_GEAR_REVERSE;
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
    rpm = 950.0f + (gearLoad * 6600.0f) + (sKartLastThrottleAmount[playerIndex] * 650.0f);
    if ((gear > KART_GEAR_FIRST) && (kart_transmission_get_speed_ratio(player) < sGearMinSpeed[gear])) {
        f32 bogMultiplier = kart_transmission_get_underspeed_bog_multiplier(kart_transmission_get_speed_ratio(player), gear);
        rpm *= 0.45f + (bogMultiplier * 0.55f);
    }
    if (clutchAmount > 0.55f) {
        rpm += (clutchAmount - 0.55f) * 3000.0f * sKartLastThrottleAmount[playerIndex];
    }
    if (sKartRpmFlareTimer[playerIndex] > 0) {
        rpm += ((f32) sKartRpmFlareTimer[playerIndex] / 18.0f) * 1800.0f;
    }
    if (rpm > 8800.0f) {
        rpm = 8800.0f;
    }

    return rpm;
}

KartShiftFeedback kart_transmission_get_shift_feedback(s32 playerIndex) {
    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);
    return sKartShiftFeedback[playerIndex];
}

s16 kart_transmission_get_shift_feedback_timer(s32 playerIndex) {
    playerIndex = kart_transmission_clamp_player_index(playerIndex);
    kart_transmission_init_player(playerIndex);
    return sKartShiftFeedbackTimer[playerIndex];
}
