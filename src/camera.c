#include <libultraship.h>
#include <stdio.h>
#include <macros.h>
#include <common_structs.h>
#include <defines.h>
#include <libc/math.h>
#include <mk64.h>

#include "camera.h"
#include "code_800029B0.h"
#include "racing/math_util.h"
#include "racing/memory.h"
#include "waypoints.h"
#include "render_player.h"
#include "collision.h"
#include "code_80057C60.h"
#include "code_80005FD0.h"
#include "main.h"
#include "spawn_players.h"
#include "kart_input.h"
#include "enhancements/freecam/freecam_engine.h"
#include "enhancements/freecam/freecam.h"
#include "port/interpolation/FrameInterpolation.h"

#include "engine/GameAPI.h"
#include "port/Game.h"

f32 D_800DDB30[] = { 0.4f, 0.6f, 0.275f, 0.3f };

Camera cameras[NUM_CAMERAS]; // This size should be 5 but there is an overflow somewhere in Bowser's Castle, so we allocate 8 cameras to avoid it.
Camera* camera1 = &cameras[0];
Camera* camera2 = &cameras[1];
Camera* camera3 = &cameras[2];
Camera* camera4 = &cameras[3];
Camera* gFreecamCamera = &cameras[4];

static f32 sArcadeKartPostFxPlayerScale[NUM_PLAYERS];
static s8 sFirstPersonViewEnabled[NUM_PLAYERS];
static f32 sFirstPersonLookYaw[NUM_PLAYERS];
static f32 sFirstPersonLookPitch[NUM_PLAYERS];

#define FIRST_PERSON_EYE_RIGHT 0.0f
#define FIRST_PERSON_EYE_UP 5.5f
#define FIRST_PERSON_EYE_FORWARD 0.0f
#define FIRST_PERSON_LOOK_RIGHT 0.0f
#define FIRST_PERSON_LOOK_UP 1.5f
#define FIRST_PERSON_LOOK_FORWARD 140.0f
#define FIRST_PERSON_LOOK_YAW_MAX_RADIANS 2.96705973f
#define FIRST_PERSON_LOOK_PITCH_MAX_RADIANS 0.43633232f
#define FIRST_PERSON_LOOK_REAR_RADIANS 3.14159265f
#define FIRST_PERSON_LOOK_LERP 0.22f
#define FIRST_PERSON_PI 3.14159265f
#define FIRST_PERSON_TWO_PI 6.28318531f

static s32 is_arcadekart_race_speed_fx_active(void) {
    return (gGamestate == RACING) &&
           ((gRaceState == RACE_IN_PROGRESS) || (gRaceState == RACE_CALCULATE_RANKS));
}

static f32 step_arcadekart_postfx_scale(s32 playerIndex, s32 finished) {
    f32 target = finished ? 0.0f : 1.0f;
    f32 step = CVarGetFloat("gArcadeKart.PostFx.FinishFadeStep", 0.05f);

    if ((playerIndex < 0) || (playerIndex >= NUM_PLAYERS)) {
        return target;
    }
    if (step < 0.0f) {
        step = 0.0f;
    }
    if (step > 1.0f) {
        step = 1.0f;
    }

    if (sArcadeKartPostFxPlayerScale[playerIndex] < target) {
        sArcadeKartPostFxPlayerScale[playerIndex] += step;
        if (sArcadeKartPostFxPlayerScale[playerIndex] > target) {
            sArcadeKartPostFxPlayerScale[playerIndex] = target;
        }
    } else if (sArcadeKartPostFxPlayerScale[playerIndex] > target) {
        sArcadeKartPostFxPlayerScale[playerIndex] -= step;
        if (sArcadeKartPostFxPlayerScale[playerIndex] < target) {
            sArcadeKartPostFxPlayerScale[playerIndex] = target;
        }
    }

    return sArcadeKartPostFxPlayerScale[playerIndex];
}

static f32 clamp_arcadekart_camera_value(f32 value, f32 minValue, f32 maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

s32 CM_IsFirstPersonViewEnabled(s32 playerIndex) {
    if ((playerIndex < 0) || (playerIndex >= NUM_PLAYERS)) {
        return 0;
    }
    return sFirstPersonViewEnabled[playerIndex] != 0;
}

void CM_DisableFirstPersonView(s32 playerIndex) {
    if ((playerIndex < 0) || (playerIndex >= NUM_PLAYERS)) {
        return;
    }
    sFirstPersonViewEnabled[playerIndex] = 0;
    sFirstPersonLookYaw[playerIndex] = 0.0f;
    sFirstPersonLookPitch[playerIndex] = 0.0f;
}

void CM_ResetFirstPersonViews(void) {
    s32 i;

    for (i = 0; i < NUM_PLAYERS; i++) {
        sFirstPersonViewEnabled[i] = 0;
        sFirstPersonLookYaw[i] = 0.0f;
        sFirstPersonLookPitch[i] = 0.0f;
    }
}

static void update_first_person_view_toggle(s32 playerIndex) {
    struct Controller* controller;

    if ((playerIndex < 0) || (playerIndex >= 4) || (gGamestate != RACING)) {
        return;
    }

    controller = &gControllers[playerIndex];
    if (kart_input_was_command_pressed(controller, KART_INPUT_TOGGLE_FIRST_PERSON)) {
        sFirstPersonViewEnabled[playerIndex] = !sFirstPersonViewEnabled[playerIndex];
        sFirstPersonLookYaw[playerIndex] = 0.0f;
        sFirstPersonLookPitch[playerIndex] = 0.0f;
        kart_input_consume_command_press(controller, KART_INPUT_TOGGLE_FIRST_PERSON);
    }
}

static f32 lerp_first_person_angle(f32 current, f32 target) {
    f32 delta = target - current;

    while (delta > FIRST_PERSON_PI) {
        delta -= FIRST_PERSON_TWO_PI;
    }
    while (delta < -FIRST_PERSON_PI) {
        delta += FIRST_PERSON_TWO_PI;
    }

    return current + (delta * FIRST_PERSON_LOOK_LERP);
}

static void apply_first_person_view_camera(Camera* camera, Player* player, s32 playerIndex) {
    Mat3 orientation;
    Vec3f localEye;
    Vec3f localLook;
    struct Controller* controller;
    f32 targetYaw;
    f32 targetPitch;
    f32 lookYaw;
    f32 lookPitch;
    f32 lookForward;
    f32 eyeX;
    f32 eyeY;
    f32 eyeZ;
    f32 lookX;
    f32 lookY;
    f32 lookZ;
    f32 dx;
    f32 dy;
    f32 dz;

    if ((camera == NULL) || (player == NULL)) {
        return;
    }

    localEye[0] = FIRST_PERSON_EYE_RIGHT;
    localEye[1] = FIRST_PERSON_EYE_UP;
    localEye[2] = FIRST_PERSON_EYE_FORWARD;

    controller = &gControllers[playerIndex];
    if ((controller->button & L_CBUTTONS) != 0) {
        targetYaw = FIRST_PERSON_LOOK_REAR_RADIANS;
        targetPitch = 0.0f;
    } else {
        targetYaw = -kart_input_get_look_right_axis(controller) * FIRST_PERSON_LOOK_YAW_MAX_RADIANS;
        targetPitch = kart_input_get_look_up_axis(controller) * FIRST_PERSON_LOOK_PITCH_MAX_RADIANS;
    }
    sFirstPersonLookYaw[playerIndex] = lerp_first_person_angle(sFirstPersonLookYaw[playerIndex], targetYaw);
    sFirstPersonLookPitch[playerIndex] =
        sFirstPersonLookPitch[playerIndex] + ((targetPitch - sFirstPersonLookPitch[playerIndex]) * FIRST_PERSON_LOOK_LERP);
    lookYaw = sFirstPersonLookYaw[playerIndex];
    lookPitch = sFirstPersonLookPitch[playerIndex];

    lookForward = cosf(lookPitch) * FIRST_PERSON_LOOK_FORWARD;
    localLook[0] = FIRST_PERSON_LOOK_RIGHT + (sinf(lookYaw) * lookForward);
    localLook[1] = localEye[1] + FIRST_PERSON_LOOK_UP + (sinf(lookPitch) * FIRST_PERSON_LOOK_FORWARD);
    localLook[2] = cosf(lookYaw) * lookForward;

    calculate_orientation_matrix(orientation, 0, 1, 0, player->rotation[1]);
    mtxf_translate_vec3f_mat3(localEye, orientation);
    mtxf_translate_vec3f_mat3(localLook, orientation);

    eyeX = player->pos[0] + localEye[0];
    eyeY = player->pos[1] + localEye[1];
    eyeZ = player->pos[2] + localEye[2];
    lookX = player->pos[0] + localLook[0];
    lookY = player->pos[1] + localLook[1];
    lookZ = player->pos[2] + localLook[2];

    camera->unk_B0 = 0;
    camera->unk_2C = player->rotation[1];
    camera->unk_AC = player->rotation[1];
    camera->pos[0] = eyeX;
    camera->pos[1] = eyeY;
    camera->pos[2] = eyeZ;
    camera->lookAt[0] = lookX;
    camera->lookAt[1] = lookY;
    camera->lookAt[2] = lookZ;

    dx = camera->lookAt[0] - camera->pos[0];
    dy = camera->lookAt[1] - camera->pos[1];
    dz = camera->lookAt[2] - camera->pos[2];
    camera->rot[1] = atan2s(dx, dz);
    camera->rot[0] = atan2s(sqrtf((dx * dx) + (dz * dz)), dy);
    camera->rot[2] = 0;

}

static f32 normalize_arcadekart_camera_range(f32 value, f32 minValue, f32 maxValue) {
    f32 range = maxValue - minValue;
    if (fabsf(range) < 0.0001f) {
        return (value >= maxValue) ? 1.0f : 0.0f;
    }
    return clamp_arcadekart_camera_value((value - minValue) / range, 0.0f, 1.0f);
}

static f32 apply_arcadekart_camera_curve(f32 value, f32 responsePower) {
    return powf(clamp_arcadekart_camera_value(value, 0.0f, 1.0f),
                clamp_arcadekart_camera_value(responsePower, 0.01f, 8.0f));
}

static f32 get_arcadekart_camera_boosted_speed_reference(Player* player) {
    f32 topGearSpeedRatio;
    f32 boostedSpeedMultiplier;
    f32 speedReference;

    if ((player == NULL) || (player->topSpeed <= 0.0f)) {
        return 1.0f;
    }

    topGearSpeedRatio = CVarGetFloat("gArcadeKart.Camera.TopGearSpeedReferenceRatio", 1.165f);
    boostedSpeedMultiplier = CVarGetFloat("gArcadeKart.Camera.BoostedSpeedReferenceMultiplier", 1.25f);
    topGearSpeedRatio = clamp_arcadekart_camera_value(topGearSpeedRatio, 1.0f, 3.0f);
    boostedSpeedMultiplier = clamp_arcadekart_camera_value(boostedSpeedMultiplier, 1.0f, 4.0f);
    speedReference = player->topSpeed * topGearSpeedRatio * boostedSpeedMultiplier;

    if (speedReference <= 1.0f) {
        return 1.0f;
    }
    return speedReference;
}

static f32 get_arcadekart_camera_speed_curve(Player* player, f32* speedForRatioOut, f32* speedReferenceOut,
                                             f32* speedRatioOut) {
    f32 speedForRatio = 0.0f;
    f32 speedReference = 1.0f;
    f32 speedRatio = 0.0f;
    f32 speedMinRatio;
    f32 speedMaxRatio;
    f32 responsePower;

    if ((player != NULL) && is_arcadekart_race_speed_fx_active()) {
        speedForRatio = fabsf(player->currentSpeed);
        speedReference = get_arcadekart_camera_boosted_speed_reference(player);
        speedRatio = speedForRatio / speedReference;
    }
    speedRatio = clamp_arcadekart_camera_value(speedRatio, 0.0f, 1.0f);

    speedMinRatio = CVarGetFloat("gArcadeKart.PostFx.SpeedMinRatio", 0.0f);
    speedMaxRatio = CVarGetFloat("gArcadeKart.PostFx.SpeedMaxRatio", 1.0f);
    responsePower = CVarGetFloat("gArcadeKart.PostFx.ResponsePower", 2.0f);

    if (speedForRatioOut != NULL) {
        *speedForRatioOut = speedForRatio;
    }
    if (speedReferenceOut != NULL) {
        *speedReferenceOut = speedReference;
    }
    if (speedRatioOut != NULL) {
        *speedRatioOut = speedRatio;
    }

    return apply_arcadekart_camera_curve(normalize_arcadekart_camera_range(speedRatio, speedMinRatio, speedMaxRatio),
                                         responsePower);
}

static void get_arcadekart_camera_speed_position_offset(Player* player, Camera* camera, Vec3f offset, f32* curveOut,
                                                        f32* backOut, f32* upOut) {
    f32 speedCurve;
    f32 nearBack;
    f32 nearUp;
    f32 farBack;
    f32 farUp;
    f32 back;
    f32 up;

    offset[0] = camera->unk_30[0];
    offset[1] = camera->unk_30[1];
    offset[2] = camera->unk_30[2];

    if ((player == NULL) || !is_arcadekart_race_speed_fx_active() ||
        (CVarGetInteger("gArcadeKart.Camera.SpeedPositionShift", 1) == 0) || (gModeSelection == BATTLE)) {
        if (curveOut != NULL) {
            *curveOut = 0.0f;
        }
        if (backOut != NULL) {
            *backOut = fabsf(offset[2]);
        }
        if (upOut != NULL) {
            *upOut = offset[1];
        }
        return;
    }

    speedCurve = get_arcadekart_camera_speed_curve(player, NULL, NULL, NULL);
    nearBack = CVarGetFloat("gArcadeKart.Camera.PositionNearBack", 45.0f);
    nearUp = CVarGetFloat("gArcadeKart.Camera.PositionNearUp", 12.0f);
    farBack = CVarGetFloat("gArcadeKart.Camera.PositionFarBack", 60.0f);
    farUp = CVarGetFloat("gArcadeKart.Camera.PositionFarUp", 3.0f);

    nearBack = clamp_arcadekart_camera_value(nearBack, 0.0f, 200.0f);
    nearUp = clamp_arcadekart_camera_value(nearUp, -50.0f, 100.0f);
    farBack = clamp_arcadekart_camera_value(farBack, 0.0f, 200.0f);
    farUp = clamp_arcadekart_camera_value(farUp, -50.0f, 100.0f);

    back = nearBack + ((farBack - nearBack) * speedCurve);
    up = nearUp + ((farUp - nearUp) * speedCurve);

    offset[1] = up;
    offset[2] = (camera->unk_30[2] < 0.0f) ? -back : back;

    if (curveOut != NULL) {
        *curveOut = speedCurve;
    }
    if (backOut != NULL) {
        *backOut = back;
    }
    if (upOut != NULL) {
        *upOut = up;
    }
}

static f32 get_arcadekart_surface_roughness_normalized(Player* player) {
    if (player == NULL) {
        return 0.0f;
    }

    switch (player->surfaceType) {
        case AIRBORNE:
            return 0.0f;
        case ICE:
            return 0.05f;
        case RAMP:
        case BOOST_RAMP_WOOD:
        case BOOST_RAMP_ASPHALT:
            return 0.1f;
        case ASPHALT:
            return 0.15f;
        case GRASS:
            return 0.3f;
        case STONE:
        case CAVE:
            return 0.4f;
        case ROPE_BRIDGE:
        case WOOD_BRIDGE:
            return 0.7f;
        case SAND_OFFROAD:
        case SNOW_OFFROAD:
            return 1.0f;
        case SAND:
        case WET_SAND:
        case SNOW:
        case CLIFF:
        case OUT_OF_BOUNDS:
            return 0.8f;
        case DIRT:
            return 0.9f;
        case DIRT_OFFROAD:
        case TRAIN_TRACK:
            return 1.0f;
        default:
            return 0.5f;
    }
}

static s32 is_arcadekart_player_boosting(Player* player) {
    if (player == NULL) {
        return 0;
    }
    return ((player->effects & (BOOST_EFFECT | BOOST_RAMP_ASPHALT_EFFECT | BOOST_RAMP_WOOD_EFFECT | STAR_EFFECT)) != 0) ||
           (player->boostPower > 1.0f);
}

UNUSED s32 D_801649D0[2];

f32 D_801649D8[NUM_CAMERAS];
f32 D_801649E8[NUM_CAMERAS];
f32 D_801649F8[NUM_CAMERAS];
s32 D_80164A08[4];
s32 D_80164A18[NUM_CAMERAS];
s32 D_80164A28;
s32 D_80164A2C;
static s32 sStagingTimer[NUM_CAMERAS];
f32 D_80164A30;
f32 D_80164A38[NUM_CAMERAS];
f32 D_80164A48[NUM_CAMERAS];
f32 D_80164A78[NUM_CAMERAS];
s8 D_80164A88;
s8 D_80164A89;
// UNUSED s8 D_80164A8C[3];
f32 D_80164A90[NUM_CAMERAS];
f32 D_80164AA0[NUM_CAMERAS];

void camera_init(Vec3f pos, s16 rot, u32 mode, s32 cameraId) {
    Camera* camera = &cameras[cameraId];

    if (cameraId >= NUM_CAMERAS) {
        return;
    }

    camera->mode = mode;
    sStagingTimer[cameraId] = 0;
    switch (mode) {
        case 0:
        case 1:
        case 3:
        case 8:
        case 9:
        case 10:
            D_80164A89 = 0;
            camera->pos[0] = pos[0];
            camera->pos[1] = pos[1];
            camera->pos[2] = pos[2];
            camera->someBitFlags = 0;
            camera->lookAt[0] = 0.0f;
            camera->lookAt[2] = 150.0f;
            camera->lookAt[1] = pos[1] - 3.0;
            camera->up[0] = 0.0f;
            camera->up[1] = 1.0f;
            camera->up[2] = 0.0f;

            camera->unk_B0 = 0;
            camera->unk_A0 = 0.0f;

            D_801649D8[cameraId] = 20.0f;
            D_801649E8[cameraId] = 10.0f;
            D_801649F8[cameraId] = 7.0f;
            D_80164A2C = 0;
            D_80164A30 = 30.0f;
            D_80164A38[cameraId] = 0.0f;
            D_80164A48[cameraId] = 0.0f;

            D_80164A90[cameraId] = 0.0f;
            D_80164AA0[cameraId] = 0.0f;
            D_80164A78[cameraId] = D_800DDB30[gActiveScreenMode];
            D_80164A18[cameraId] = 0;
            //D_80164A08[cameraId] = 0; // Now reset in spawn_players_and_cameras
            //D_80164498[cameraId] = 0.0f;
            camera->unk_94.unk_8 = 0;
            camera->unk_94.unk_0 = 0.0f;

            camera->unk_2C = rot;
            camera->unk_AC = rot;
            switch (gActiveScreenMode) {
                case SCREEN_MODE_1P:
                case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
                    if (gModeSelection == BATTLE) {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 11.6f;
                        camera->unk_30[2] = -38.5f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 19.2f;
                        D_80164A88 = 0;
                    } else {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 9.5f;
                        camera->unk_30[2] = -50.0f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 70.0f;
                    }
                    break;
                case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
                    if (gModeSelection == BATTLE) {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 11.6f;
                        camera->unk_30[2] = -38.5f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 19.2f;
                    } else {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 9.6f;
                        camera->unk_30[2] = -35.0f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 30.0f;
                    }
                    break;
                case SCREEN_MODE_3P_4P_SPLITSCREEN:
                    if (gModeSelection == BATTLE) {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 11.6f;
                        camera->unk_30[2] = -38.5f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 19.2f;
                    } else {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 9.0f;
                        camera->unk_30[2] = -40.0f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 18.0f;
                    }
                    break;
            }

            func_80014DE4(cameraId);

            camera->fieldOfView = 40.0f;
            camera->unk_B4 = camera->fieldOfView;
            if (D_80164678[cameraId] == 2) {
                D_80164A38[cameraId] = 20.0f;
                D_80164A48[cameraId] = 1.5f;
                D_80164A78[cameraId] = 1.0f;
            }
            break;
    }

    func_802B7F7C(camera->pos, camera->lookAt, camera->rot);
}

// Many arrays are hard-coded to 4. Skip those.
void freecam_init(Vec3f pos, s16 rot, u32 mode, s32 cameraId) {
    Camera* camera = &cameras[cameraId];

    if (cameraId >= NUM_CAMERAS) {
        return;
    }

    camera->fieldOfView = 40.0f;
    camera->mode = mode;
    sStagingTimer[cameraId] = 0;
    switch (mode) {
        case 0:
        case 1:
        case 3:
        case 8:
        case 9:
        case 10:
            D_80164A89 = 0;
            camera->pos[0] = pos[0];
            camera->pos[1] = pos[1];
            camera->pos[2] = pos[2];
            camera->someBitFlags = 0;
            camera->lookAt[0] = 0.0f;
            camera->lookAt[2] = 150.0f;
            camera->lookAt[1] = pos[1] - 3.0;
            camera->up[0] = 0.0f;
            camera->up[1] = 1.0f;
            camera->up[2] = 0.0f;
            //camera->playerId = (s16) cameraId;
            camera->unk_B0 = 0;
            camera->unk_A0 = 0.0f;

            // D_801649D8[cameraId] = 20.0f;
            // D_801649E8[cameraId] = 10.0f;
            // D_801649F8[cameraId] = 7.0f;
            // D_80164A2C = 0;
            // D_80164A30 = 30.0f;
            // D_80164A38[cameraId] = 0.0f;
            // D_80164A48[cameraId] = 0.0f;

            // D_80164A90[cameraId] = 0.0f;
            // D_80164AA0[cameraId] = 0.0f;
            // D_80164A78[cameraId] = D_800DDB30[gActiveScreenMode];
            // D_80164A18[cameraId] = 0;
            // D_80164A08[cameraId] = 0;
            // D_80164498[cameraId] = 0.0f;
            camera->unk_94.unk_8 = 0;
            camera->unk_94.unk_0 = 0.0f;

            camera->unk_2C = rot;
            camera->unk_AC = rot;
            switch (gActiveScreenMode) {
                case SCREEN_MODE_1P:
                case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
                    if (gModeSelection == BATTLE) {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 11.6f;
                        camera->unk_30[2] = -38.5f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 19.2f;
                        D_80164A88 = 0;
                    } else {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 9.5f;
                        camera->unk_30[2] = -50.0f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 70.0f;
                    }
                    break;
                case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
                    if (gModeSelection == BATTLE) {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 11.6f;
                        camera->unk_30[2] = -38.5f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 19.2f;
                    } else {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 9.6f;
                        camera->unk_30[2] = -35.0f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 30.0f;
                    }
                    break;
                case SCREEN_MODE_3P_4P_SPLITSCREEN:
                    if (gModeSelection == BATTLE) {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 11.6f;
                        camera->unk_30[2] = -38.5f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 19.2f;
                    } else {
                        camera->unk_30[0] = 0.0f;
                        camera->unk_30[1] = 9.0f;
                        camera->unk_30[2] = -40.0f;
                        camera->unk_3C[0] = 0.0f;
                        camera->unk_3C[1] = 0.0f;
                        camera->unk_3C[2] = 18.0f;
                    }
                    break;
            }

            //func_80014DE4(cameraId);

           // if (D_80164678[cameraId] == 0) {
                if (D_80164A28 == 1) {
                  //  camera->fieldOfView = 80.0f;
                } else {
                   // camera->fieldOfView = 40.0f;
                }
                camera->unk_B4 = camera->fieldOfView;
           // }
            // if (D_80164678[cameraId] == 1) {
            //     if (D_80164A28 == 1) {
            //         camera->fieldOfView = 100.0f;
            //     } else {
            //         camera->fieldOfView = 60.0f;
            //     }
            //     camera->unk_B4 = camera->fieldOfView;
            // // }
            // if (D_80164678[cameraId] == 2) {
            //     if (D_80164A28 == 1) {
            //         camera->fieldOfView = 100.0f;
            //     } else {
            //         camera->fieldOfView = 60.0f;
            //     }
            //     camera->unk_B4 = camera->fieldOfView;
            //     D_80164A38[cameraId] = 20.0f;
            //     D_80164A48[cameraId] = 1.5f;
            //     D_80164A78[cameraId] = 1.0f;
            // }
            break;
    }
    func_802B7F7C(camera->pos, camera->lookAt, camera->rot);
}

// Thwomp related
void func_8001CA10(Camera* camera) {
    camera->unk_94.unk_8 = 0;
    camera->unk_94.unk_0 = 6.0f;
}

void func_8001CA24(Player* player, f32 arg1) {
    Camera* camera = CM_GetPlayerCamera(player - gPlayerOne);

    if (NULL == camera) {
        printf("[camera.c][func_8001CA24] Could not find a camera using GetPlayerCamera()\n");
        return;
    }

    camera->unk_94.unk_8 = 0;
    camera->unk_94.unk_0 = arg1;
}

void func_8001CA78(UNUSED Player* player, Camera* camera, Vec3f arg2, f32* arg3, f32* arg4, f32* arg5, UNUSED s32 huh,
                   UNUSED s32 wut) {
    Mat3 sp74;
    Vec3f sp68;
    Vec3f sp5C;
    f32 posX;
    f32 posY;
    f32 posZ;
    f32 var_f14;
    f32 temp_f18;
    f32 temp_f16;
    UNUSED s32 pad;
    TrackPathPoint* temp_s2;

    temp_s2 = &gTrackPaths[0][gPathCountByPathIndex[0] - 10];
    sp68[0] = camera->unk_30[0];
    sp68[1] = camera->unk_30[1];
    sp68[2] = camera->unk_30[2];
    sp5C[0] = camera->unk_3C[0];
    sp5C[1] = camera->unk_3C[1];
    sp5C[2] = camera->unk_3C[2];
    arg2[0] = camera->lookAt[0];
    arg2[1] = camera->lookAt[1];
    arg2[2] = camera->lookAt[2];
    calculate_orientation_matrix(sp74, 0, 1, 0, -0x00008000);
    mtxf_translate_vec3f_mat3(sp5C, sp74);
    if (IsToadsTurnpike()) {
        var_f14 = sp5C[0];
    } else {
        var_f14 = sp5C[0] + temp_s2->x;
    }
    temp_f16 = D_80165230[7] + sp5C[2];
    temp_f18 = sp5C[1] + (temp_s2->y + D_80164A30);
    arg2[0] += (var_f14 - camera->lookAt[0]) * 1;
    arg2[1] += (temp_f18 - camera->lookAt[1]) * 1;
    arg2[2] += (temp_f16 - camera->lookAt[2]) * 1;
    mtxf_translate_vec3f_mat3(sp68, sp74);
    if (IsToadsTurnpike()) {
        var_f14 = sp68[0];
    } else {
        var_f14 = sp68[0] + temp_s2->x;
    }
    temp_f16 = D_80165230[7] + sp68[2];
    temp_f18 = sp68[1] + (temp_s2->y + D_80164A30 + 6.0f);
    move_f32_towards(&D_80164A30, 0, 0.02f);
    posX = camera->pos[0];
    *arg3 = ((var_f14 - posX) * 1) + posX;
    posY = camera->pos[1];
    *arg4 = ((temp_f18 - posY) * 1) + posY;
    posZ = camera->pos[2];
    *arg5 = ((temp_f16 - posZ) * 1) + posZ;
}

void func_8001CCEC(Player* player, Camera* camera, Vec3f arg2, f32* arg3, f32* arg4, f32* arg5, UNUSED s32* arg6,
                   s16 arg7, s16 index) {
    Mat3 sp9C;
    Vec3f sp90;
    Vec3f sp84;
    UNUSED s32 pad[3];
    f32 x;
    f32 y;
    f32 z;
    UNUSED s32 pad2;
    f32 var_f2;
    s16 var_v1;
    f32 temp_f0;
    f32 var_f0;
    s16 var_v0;
    f32 temp_f12;
    Vec3f cameraPositionOffset;
    f32 cameraPositionCurve;
    f32 cameraPositionBack;
    f32 cameraPositionUp;

    var_v1 = player->unk_DB4.unk0;
    var_f2 = player->unk_DB4.unk8;

    var_v1++;
    temp_f0 = ((var_f2 * var_v1) - (0.7 * (var_v1 * var_v1)));
    if ((var_v1 != 0) && (temp_f0 < 0)) {
        var_v1 = 0;
        var_f2 *= 0.8;
        if (var_f2 <= 0.1) {
            var_f2 = 0;
        }
    }
    if (temp_f0 <= 0) {
        temp_f0 = 0;
    }
    player->unk_DB4.unk0 = var_v1;
    player->unk_DB4.unk8 = var_f2;
    var_v0 = camera->unk_94.unk_8;
    var_f0 = camera->unk_94.unk_0;
    var_v0++;
    temp_f12 = (var_v0 * var_f0) - (1.25 * (var_v0 * var_v0));
    if ((var_v0 != 0) && (temp_f12 < 0)) {
        var_v0 = 0;
        var_f0 *= 0.9;
        if (var_f0 <= 0.1) {
            var_f0 = 0;
        }
    }
    if (temp_f12 <= 0) {
        temp_f12 = 0;
        // fakematch
        if (!var_v0) {}
    }
    camera->unk_94.unk_8 = var_v0;
    camera->unk_94.unk_0 = var_f0;
    if (D_80164678[index] == 2) {
        move_f32_towards(&D_80164A38[index], 20.0f, 0.1f);
        move_f32_towards(&D_80164A48[index], 1.5f, 0.1f);
        D_80164A78[index] += 0.1;
        if (D_80164A78[index] >= 1) {
            D_80164A78[index] = 1;
        }

    } else {
        move_f32_towards(&D_80164A38[index], 0, 0.1f);
        move_f32_towards(&D_80164A48[index], 0, 0.1f);
        D_80164A78[index] -= 0.1;
        if (D_800DDB30[gActiveScreenMode] >= D_80164A78[index]) {
            D_80164A78[index] = D_800DDB30[gActiveScreenMode];
        }
    }
    if ((player->lakituProps & WENT_OVER_OOB) == WENT_OVER_OOB) {
        switch (gActiveScreenMode) {
            case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
            case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
            case SCREEN_MODE_3P_4P_SPLITSCREEN:
                move_f32_towards(&D_80164A90[index], 20, 0.02f);
                move_f32_towards(&D_80164AA0[index], 10, 0.02f);
                break;
            default:
                if (IsYoshiValley()) {
                    move_f32_towards(&D_80164A90[index], 50, 0.04f);
                    move_f32_towards(&D_80164AA0[index], 35, 0.04f);
                } else {
                    move_f32_towards(&D_80164A90[index], 40, 0.02f);
                    move_f32_towards(&D_80164AA0[index], 20, 0.02f);
                }
                break;
        }
    } else {
        move_f32_towards(&D_80164A90[index], 0, 0.04f);
        move_f32_towards(&D_80164AA0[index], 0, 0.04f);
    }
    get_arcadekart_camera_speed_position_offset(player, camera, cameraPositionOffset, &cameraPositionCurve,
                                                &cameraPositionBack, &cameraPositionUp);
    if (index == 0) {
        CVarSetFloat("gArcadeKart.Camera.DebugPositionCurve", cameraPositionCurve);
        CVarSetFloat("gArcadeKart.Camera.DebugPositionBack", cameraPositionBack);
        CVarSetFloat("gArcadeKart.Camera.DebugPositionUp", cameraPositionUp);
    }

    sp90[0] = cameraPositionOffset[0];
    sp90[1] = cameraPositionOffset[1] + (player->unk_DB4.unk1E * 0.85) - D_80164A48[index] + D_80164AA0[index] +
              (temp_f12 / 2);
    sp90[2] = cameraPositionOffset[2] + temp_f0 + D_80164A38[index];
    sp84[0] = camera->unk_3C[0];
    sp84[1] = camera->unk_3C[1] + (player->unk_DB4.unk1E * 0.85) + temp_f12;
    sp84[2] = camera->unk_3C[2] + temp_f0 - D_80164A90[index];
    arg2[0] = camera->lookAt[0];
    arg2[1] = camera->lookAt[1];
    arg2[2] = camera->lookAt[2];
    if ((player->effects & 0x01000000) == 0x01000000) {
        sp84[2] /= 3.0f;
    }
    calculate_orientation_matrix(sp9C, 0, 1, 0, arg7);
    mtxf_translate_vec3f_mat3(sp84, sp9C);

    x = player->pos[0] + sp84[0];
    z = player->pos[2] + sp84[2];
    y = player->pos[1] + sp84[1];

    arg2[0] += (x - camera->lookAt[0]) * D_80164A78[index];
    arg2[2] += ((z - camera->lookAt[2]) * D_80164A78[index]);

    if ((((player->speed / 18) * 216) <= 5.0f) && ((player->effects & 2) == 2)) {
        arg2[1] += ((y - camera->lookAt[1]) * 0.02);
    } else {
        arg2[1] += ((y - camera->lookAt[1]) * 0.5);
    }
    mtxf_translate_vec3f_mat3(sp90, sp9C);
    x = player->pos[0] + sp90[0];
    z = player->pos[2] + sp90[2];
    if ((player->effects & 0x01000000) != 0x01000000) {
        var_f0 = player->pos[1] + sp90[1];
        // permute
        y = var_f0;
    } else {
        y = player->unk_074 + player->boundingBoxSize + sp90[1];
    }

    *arg3 = camera->pos[0] + ((x - camera->pos[0]) * D_80164A78[index]);
    *arg5 = camera->pos[2] + ((z - camera->pos[2]) * D_80164A78[index]);

    if ((((player->speed / 18) * 216) <= 5.0f) && ((player->effects & 2) == 2)) {
        *arg4 = camera->pos[1] + (((y - camera->pos[1]) * 0.01));
    } else {
        *arg4 = camera->pos[1] + (((y - camera->pos[1]) * 0.15));
    }

    if ((player->oobProps & UNDER_OOB_OR_FLUID_LEVEL) != 0) {
        *arg4 = gPlayerWaterLevel[index];
    }
}

void func_8001D53C(Player* player, Camera* camera, Vec3f arg2, f32* arg3, f32* arg4, f32* arg5, s16 arg6, s16 arg7) {
    Mat3 sp74;
    Vec3f sp68;
    Vec3f sp5C;
    f32 stackPadding0;
    f32 stackPadding1;
    f32 stackPadding2;
    UNUSED f32 pad[4];
    f32 thing;

    if (((u16) player->unk_222 == 0) && (camera->unk_A0 == 0.0f)) {
        camera->unk_A0 = 0.0f;
    }
    if ((u16) player->unk_222 != 4) {
        move_f32_towards(&camera->unk_A0, 20.0f, 0.06f);
    } else {
        move_f32_towards(&camera->unk_A0, 0.0f, 0.06f);
    }
    thing = gPlayerWaterLevel[arg7];
    sp68[0] = camera->unk_30[0];
    sp68[1] = camera->unk_30[1];
    sp68[2] = camera->unk_30[2];
    sp5C[0] = camera->unk_3C[0];
    sp5C[1] = camera->unk_3C[1] + camera->unk_A0;
    sp5C[2] = camera->unk_3C[2];
    arg2[0] = camera->lookAt[0];
    arg2[1] = camera->lookAt[1];
    arg2[2] = camera->lookAt[2];
    calculate_orientation_matrix(sp74, 0.0f, 1.0f, 0.0f, arg6);
    mtxf_translate_vec3f_mat3(sp5C, sp74);
    stackPadding0 = player->pos[0] + sp5C[0];
    stackPadding2 = player->pos[2] + sp5C[2];
    stackPadding1 = player->pos[1] + sp5C[1];
    arg2[0] += (stackPadding0 - camera->lookAt[0]) * 1;
    arg2[2] += (stackPadding2 - camera->lookAt[2]) * 1;
    arg2[1] += (stackPadding1 - camera->lookAt[1]) * 1;
    mtxf_translate_vec3f_mat3(sp68, sp74);
    stackPadding0 = player->pos[0] + sp68[0];
    stackPadding2 = player->pos[2] + sp68[2];
    stackPadding1 = sp68[1] + (player->unk_074 + 1.5);
    if ((player->lakituProps & LAKITU_RETRIEVAL) == LAKITU_RETRIEVAL) {
        stackPadding1 = sp68[1] + (thing + 10.0f);
    }
    *arg3 = stackPadding0;
    *arg4 = stackPadding1;
    *arg5 = stackPadding2;
    D_80164A90[arg7] = 0.0f;
    D_80164AA0[arg7] = 0.0f;
}

void func_8001D794(Player* player, Camera* camera, Vec3f arg2, f32* arg3, f32* arg4, f32* arg5, s16 arg6) {
    Mat3 sp6C;
    Vec3f sp60;
    Vec3f sp54;
    UNUSED f32 stackPadding[4];
    f32 test1;
    f32 test2;
    f32 test3;

    sp60[0] = camera->unk_30[0];
    sp60[1] = camera->unk_30[1];
    sp60[2] = camera->unk_30[2] - 6;

    sp54[0] = camera->unk_3C[0];
    sp54[1] = camera->unk_3C[1];
    sp54[2] = camera->unk_3C[2];

    arg2[0] = camera->lookAt[0];
    arg2[1] = camera->lookAt[1];
    arg2[2] = camera->lookAt[2];

    calculate_orientation_matrix(sp6C, 0, 1, 0, arg6);
    mtxf_translate_vec3f_mat3(sp54, sp6C);

    test1 = player->pos[0] + sp54[0];
    test3 = player->pos[2] + sp54[2];
    test2 = player->pos[1] + sp54[1];
    arg2[0] += (test1 - camera->lookAt[0]) * 1;
    arg2[1] += (test2 - camera->lookAt[1]) * 1;
    arg2[2] += (test3 - camera->lookAt[2]) * 1;

    mtxf_translate_vec3f_mat3(sp60, sp6C);

    test1 = player->pos[0] + sp60[0];
    test3 = player->pos[2] + sp60[2];
    test2 = player->pos[1] + sp60[1];
    *arg3 = camera->pos[0] + ((test1 - camera->pos[0]) * 1);
    *arg4 = camera->pos[1] + ((test2 - camera->pos[1]) * 1);
    *arg5 = camera->pos[2] + ((test3 - camera->pos[2]) * 1);
}

void func_8001D944(Player* player, Camera* camera, Vec3f arg2, f32* arg3, f32* arg4, f32* arg5, UNUSED s32* arg6,
                   s16 arg7, s16 index) {
    Mat3 sp9C;
    Vec3f sp90;
    Vec3f sp84;
    UNUSED s32 pad[3];
    f32 x;
    f32 y;
    f32 z;
    UNUSED s32 pad2;
    f32 var_f2;
    s16 var_v1;
    f32 temp_f0;
    f32 var_f0;
    s16 var_v0;
    f32 temp_f12;

    var_v1 = player->unk_DB4.unk0;
    var_f2 = player->unk_DB4.unk8;

    var_v1++;
    temp_f0 = ((var_f2 * var_v1) - (0.7 * (var_v1 * var_v1)));
    if ((var_v1 != 0) && (temp_f0 < 0)) {
        var_v1 = 0;
        var_f2 *= 0.8;
        if (var_f2 <= 0.1) {
            var_f2 = 0;
        }
    }
    if (temp_f0 <= 0) {
        temp_f0 = 0;
    }
    player->unk_DB4.unk0 = var_v1;
    player->unk_DB4.unk8 = var_f2;
    var_v0 = camera->unk_94.unk_8;
    var_f0 = camera->unk_94.unk_0;
    var_v0++;
    temp_f12 = (var_v0 * var_f0) - (1.25 * (var_v0 * var_v0));
    if ((var_v0 != 0) && (temp_f12 < 0)) {
        var_v0 = 0;
        var_f0 *= 0.9;
        if (var_f0 <= 0.1) {
            var_f0 = 0;
        }
    }
    if (temp_f12 <= 0) {
        temp_f12 = 0;
        // fakematch
        if (!var_v0) {}
    }
    camera->unk_94.unk_8 = var_v0;
    camera->unk_94.unk_0 = var_f0;
    if (D_80164678[index] == 2) {
        move_f32_towards(&D_80164A38[index], 20.0f, 0.1f);
        move_f32_towards(&D_80164A48[index], 1.5f, 0.1f);
        D_80164A78[index] += 0.1;
        if (D_80164A78[index] >= 1) {
            D_80164A78[index] = 1;
        }

    } else {
        move_f32_towards(&D_80164A38[index], 0, 0.1f);
        move_f32_towards(&D_80164A48[index], 0, 0.1f);
        D_80164A78[index] -= 0.1;
        if (D_800DDB30[gActiveScreenMode] >= D_80164A78[index]) {
            D_80164A78[index] = D_800DDB30[gActiveScreenMode];
        }
    }
    if ((player->lakituProps & WENT_OVER_OOB) == WENT_OVER_OOB) {

        move_f32_towards(&D_80164A90[index], 15, 0.02f);
        move_f32_towards(&D_80164AA0[index], 20, 0.02f);
    } else {
        move_f32_towards(&D_80164A90[index], 0, 0.02f);
        move_f32_towards(&D_80164AA0[index], 0, 0.02f);
    }
    sp90[0] = camera->unk_30[0];
    sp90[1] =
        camera->unk_30[1] + (player->unk_DB4.unk1E * 0.85) - D_80164A48[index] + D_80164AA0[index] + (temp_f12 / 2);
    sp90[2] = camera->unk_30[2] + temp_f0 + D_80164A38[index] + D_80164AA0[index];
    sp84[0] = camera->unk_3C[0];
    sp84[1] = camera->unk_3C[1] + (player->unk_DB4.unk1E * 0.85) + temp_f12;
    sp84[2] = camera->unk_3C[2] + temp_f0 - D_80164A90[index];
    arg2[0] = camera->lookAt[0];
    arg2[1] = camera->lookAt[1];
    arg2[2] = camera->lookAt[2];
    if ((player->effects & 0x01000000) == 0x01000000) {
        sp84[2] /= 3.0f;
    }
    calculate_orientation_matrix(sp9C, 0, 1, 0, arg7);
    mtxf_translate_vec3f_mat3(sp84, sp9C);

    x = player->pos[0] + sp84[0];
    z = player->pos[2] + sp84[2];
    y = player->pos[1] + sp84[1];

    arg2[0] += (x - camera->lookAt[0]) * D_80164A78[index];
    arg2[2] += ((z - camera->lookAt[2]) * D_80164A78[index]);

    if ((((player->speed / 18) * 216) <= 5.0f) && ((player->effects & 2) == 2)) {
        arg2[1] += ((y - camera->lookAt[1]) * 0.02);
    } else {
        arg2[1] += ((y - camera->lookAt[1]) * 0.5);
    }
    mtxf_translate_vec3f_mat3(sp90, sp9C);
    x = player->pos[0] + sp90[0];
    z = player->pos[2] + sp90[2];
    if ((player->effects & 0x01000000) != 0x01000000) {
        var_f0 = player->pos[1] + sp90[1];
        // permute
        y = var_f0;
    } else {
        y = player->unk_074 + player->boundingBoxSize + sp90[1];
    }

    *arg3 = camera->pos[0] + ((x - camera->pos[0]) * D_80164A78[index]);
    *arg5 = camera->pos[2] + ((z - camera->pos[2]) * D_80164A78[index]);

    if ((((player->speed / 18) * 216) <= 5.0f) && ((player->effects & 2) == 2)) {
        *arg4 = camera->pos[1] + (((y - camera->pos[1]) * 0.01));
    } else {
        *arg4 = camera->pos[1] + (((y - camera->pos[1]) * 0.15));
    }

    if ((player->oobProps & UNDER_OOB_OR_FLUID_LEVEL) != 0) {
        *arg4 = gPlayerWaterLevel[index];
    }
}

void func_8001E0C4(Camera* camera, Player* player, s8 arg2) {
    UNUSED s32 pad[6];
    f32 temp_f12;
    f32 sp80;
    f32 temp_f14;
    UNUSED s32 pad2;
    f32 sp74;
    f32 sp70;
    f32 sp6C;
    Vec3f sp60;
    s16 temp_t7;
    s16 var_a2;
    UNUSED s32 pad3[8];
    s32 test = 3;

    if (player->unk_078 == 0) {
        var_a2 = 0x0064;
    } else if (player->unk_078 < 0) {
        var_a2 = 0x87 - (player->unk_078 / 3);
    } else {
        var_a2 = (player->unk_078 / 3) + 0x87;
    }
    adjust_angle(&camera->unk_2C, player->rotation[1], var_a2);
    func_8001CA78(player, camera, sp60, &sp74, &sp70, &sp6C, camera->unk_2C, arg2);
    camera->someBitFlags &= ~0x0004;
    temp_t7 = check_bounding_collision(&camera->collision, test, sp74, sp70, sp6C);
    if (camera->collision.surfaceDistance[2] < 0.0f) {
        sp74 += -camera->collision.orientationVector[0] * camera->collision.surfaceDistance[2] * 1;
        sp70 += -camera->collision.orientationVector[1] * camera->collision.surfaceDistance[2] * 0.5;
        sp6C += -camera->collision.orientationVector[2] * camera->collision.surfaceDistance[2] * 1;
    }
    if (camera->collision.surfaceDistance[0] < 0.0f) {
        camera->someBitFlags = camera->someBitFlags | 4 | 2;
        sp74 += -camera->collision.unk48[0] * camera->collision.surfaceDistance[0] * 1.5;
        sp70 += -camera->collision.unk48[1] * camera->collision.surfaceDistance[0] * 1;
        sp6C += -camera->collision.unk48[2] * camera->collision.surfaceDistance[0] * 1.5;
    }
    if (camera->collision.surfaceDistance[1] < 0.0f) {
        camera->someBitFlags = camera->someBitFlags | 4 | 2;
        sp74 += -camera->collision.unk54[0] * camera->collision.surfaceDistance[1] * 1.5;
        sp70 += -camera->collision.unk54[1] * camera->collision.surfaceDistance[1] * 1;
        sp6C += -camera->collision.unk54[2] * camera->collision.surfaceDistance[1] * 1.5;
    }
    if ((temp_t7 == 0) && ((camera->someBitFlags & 2) != 2)) {
        camera->unk_AC = camera->unk_2C;
    }
    camera->lookAt[0] = sp60[0];
    camera->lookAt[1] = sp60[1];
    camera->lookAt[2] = sp60[2];
    camera->pos[0] = sp74;
    camera->pos[1] = sp70;
    camera->pos[2] = sp6C;
    temp_f12 = camera->lookAt[0] - camera->pos[0];
    sp80 = camera->lookAt[1] - camera->pos[1];
    temp_f14 = camera->lookAt[2] - camera->pos[2];
    camera->rot[1] = atan2s(temp_f12, temp_f14);
    camera->rot[0] = atan2s(sqrtf((temp_f12 * temp_f12) + (temp_f14 * temp_f14)), sp80);
    camera->rot[2] = 0;
}

// This function has a few stack variables.
void func_8001E45C(Camera* camera, Player* player, s8 arg2) {
    UNUSED s32 pad[6];
    f32 temp_f12;
    f32 sp90;
    f32 temp_f14;
    UNUSED s32 pad2;
    f32 sp84;
    f32 sp80;
    f32 sp7C;
    UNUSED s32 pad3[3];
    Vec3f sp64;
    UNUSED s32 pad4[2];
    s32 sp58;
    UNUSED s16 pad5[4];
    s16 var_a3;
    UNUSED s16 pad6;
    s16 temp;

    if ((player->effects & DRIFTING_EFFECT) == DRIFTING_EFFECT) {
        var_a3 = 100;
        if (player->unk_078 == 0) {
            camera->unk_B0 = 0;
        } else {
            if (player->unk_078 < 0) {
                var_a3 = 0xA5 - (player->unk_078 / 2);
                if ((player->effects & 0x20000000) == 0x20000000) {
                    move_s16_towards(&camera->unk_B0, -0x0B60, 0.1f);
                } else {
                    move_s16_towards(&camera->unk_B0, -0x0888, 0.1f);
                }
            } else {
                var_a3 = (player->unk_078 / 2) + 0xA5;
                if ((player->effects & 0x20000000) == 0x20000000) {
                    move_s16_towards(&camera->unk_B0, 0x0B60, 0.1f);
                } else {
                    move_s16_towards(&camera->unk_B0, 0x0888, 0.1f);
                }
            }
        }
    } else {
        move_s16_towards(&camera->unk_B0, 0, 0.05f);
        var_a3 = ((s16) camera->unk_2C / 182) - ((s16) player->rotation[1] / 182);
        if (player->unk_078 == 0) {
            if ((player->effects & 0x20) == 0x20) {
                var_a3 = 0x02D8;
            } else {
                var_a3 = 0x01F4;
            }
        } else if (player->unk_078 < 0) {
            if ((var_a3 <= -70) || (var_a3 >= 70)) {
                var_a3 = 0xB4 - player->unk_078;
            } else {
                var_a3 = 0xA5 - (player->unk_078 / 2);
            }
        } else if ((var_a3 <= -70) || (var_a3 >= 0x46)) {
            var_a3 = player->unk_078 + 0xB4;
        } else {
            var_a3 = (player->unk_078 / 2) + 0xA5;
        }
    }
    if (((player->effects & 0x80) == 0x80) || ((player->effects & 0x40) == 0x40) ||
        ((player->effects & 0x4000) == 0x4000) || ((player->effects & 0x80000) == 0x80000) ||
        ((player->effects & 0x800000) == 0x800000) || (((player->effects & 0x20) == 0x20) && (player->unk_078 != 0)) ||
        (player->collision.surfaceDistance[0] <= 0.0f) || (player->collision.surfaceDistance[1] <= 0.0f) ||
        ((player->effects & 0x20000) == 0x20000)) {
        func_8001CCEC(player, camera, sp64, &sp84, &sp80, &sp7C, &sp58, (s32) camera->unk_2C, (s32) arg2);
    } else {
        adjust_angle(&camera->unk_2C, (s16) (player->rotation[1] + camera->unk_B0), var_a3);
        func_8001CCEC(player, camera, sp64, &sp84, &sp80, &sp7C, &sp58, (s32) camera->unk_2C, (s32) arg2);
    }
    temp = 3;
    camera->someBitFlags &= 0xFFFB;
    check_bounding_collision(&camera->collision, temp, sp84, sp80, sp7C);

    camera->pos[0] = sp84;
    camera->pos[1] = sp80;
    camera->pos[2] = sp7C;

    camera->lookAt[0] = sp64[0];
    camera->lookAt[1] = sp64[1];
    camera->lookAt[2] = sp64[2];

    temp_f12 = camera->lookAt[0] - camera->pos[0];
    sp90 = camera->lookAt[1] - camera->pos[1];
    temp_f14 = camera->lookAt[2] - camera->pos[2];

    camera->rot[1] = atan2s(temp_f12, temp_f14);
    camera->rot[0] = atan2s(sqrtf((temp_f12 * temp_f12) + (temp_f14 * temp_f14)), sp90);
    camera->rot[2] = 0;
}

void func_8001E8E8(Camera* camera, Player* player, s8 arg2) {
    UNUSED f32 pad[6];
    f32 temp_f12;
    f32 sp88;
    f32 temp_f14;
    UNUSED f32 pad2;
    f32 sp7C;
    f32 sp78;
    f32 sp74;
    UNUSED Vec3f pad3;
    Vec3f sp5C;
    UNUSED f32 pad4[10];

    camera->unk_B0 = 0;
    camera->unk_2C = player->rotation[1];
    func_8001D53C(player, camera, sp5C, &sp7C, &sp78, &sp74, (s16) (s32) player->rotation[1], (s16) (s32) arg2);
    check_bounding_collision(&camera->collision, 5.0f, sp7C, sp78, sp74);
    camera->lookAt[0] = sp5C[0];
    camera->lookAt[1] = sp5C[1];
    camera->lookAt[2] = sp5C[2];
    camera->pos[0] = sp7C;
    camera->pos[1] = sp78;
    camera->pos[2] = sp74;
    temp_f12 = camera->lookAt[0] - camera->pos[0];
    sp88 = camera->lookAt[1] - camera->pos[1];
    temp_f14 = camera->lookAt[2] - camera->pos[2];
    camera->rot[1] = atan2s(temp_f12, temp_f14);
    camera->rot[0] = atan2s(sqrtf((temp_f12 * temp_f12) + (temp_f14 * temp_f14)), sp88);
    camera->rot[2] = 0;
}

void func_8001EA0C(Camera* camera, Player* player, s8 arg2) {
    UNUSED s32 pad[6];
    f32 temp_f12;
    f32 sp90;
    f32 temp_f14;
    UNUSED s32 pad2;
    f32 sp84;
    f32 sp80;
    f32 sp7C;
    UNUSED s32 pad3[3];
    Vec3f sp64;
    UNUSED s32 pad4[2];
    s32 sp58;
    UNUSED s16 pad5[4];
    s16 var_a3;
    UNUSED s16 pad6;
    s16 temp;

    if ((player->effects & DRIFTING_EFFECT) == DRIFTING_EFFECT) {
        var_a3 = 100;
        if (player->unk_078 == 0) {
            camera->unk_B0 = 0;
        } else {
            if (player->unk_078 < 0) {
                var_a3 = 0xA5 - (player->unk_078 / 2);

                if ((player->effects & 0x20000000) == 0x20000000) {
                    move_s16_towards(&camera->unk_B0, -0x0B60, 0.1f);
                } else {
                    move_s16_towards(&camera->unk_B0, -0x0888, 0.1f);
                }
            } else {
                var_a3 = (player->unk_078 / 2) + 0xA5;
                if ((player->effects & 0x20000000) == 0x20000000) {
                    move_s16_towards(&camera->unk_B0, 0x0B60, 0.1f);
                } else {
                    move_s16_towards(&camera->unk_B0, 0x0888, 0.1f);
                }
            }
        }
    } else {
        move_s16_towards(&camera->unk_B0, 0, 0.05f);
        var_a3 = ((s16) camera->unk_2C / 182) - ((s16) player->rotation[1] / 182);
        if (player->unk_078 == 0) {
            if ((player->effects & 0x20) == 0x20) {
                var_a3 = 0x02D8;
            } else {
                var_a3 = 0x01F4;
            }
        } else if (player->unk_078 < 0) {
            if ((var_a3 <= -70) || (var_a3 >= 70)) {
                var_a3 = 0xB4 - player->unk_078;
            } else {
                var_a3 = 0xA5 - (player->unk_078 / 2);
            }
        } else if ((var_a3 <= -70) || (var_a3 >= 0x46)) {
            var_a3 = player->unk_078 + 0xB4;
        } else {
            var_a3 = (player->unk_078 / 2) + 0xA5;
        }
    }
    if (((player->effects & 0x80) == 0x80) || ((player->effects & 0x40) == 0x40) ||
        ((player->effects & 0x4000) == 0x4000) || ((player->effects & 0x80000) == 0x80000) ||
        ((player->effects & 0x800000) == 0x800000) || (((player->effects & 0x20) == 0x20) && (player->unk_078 != 0)) ||
        (player->collision.surfaceDistance[0] <= 0.0f) || (player->collision.surfaceDistance[1] <= 0.0f) ||
        ((player->effects & 0x20000) == 0x20000)) {
        func_8001D944(player, camera, sp64, &sp84, &sp80, &sp7C, &sp58, (s32) camera->unk_2C, (s32) arg2);
    } else {
        adjust_angle(&camera->unk_2C, (s16) (player->rotation[1] + camera->unk_B0), var_a3);
        func_8001D944(player, camera, sp64, &sp84, &sp80, &sp7C, &sp58, (s32) camera->unk_2C, (s32) arg2);
    }
    temp = 3;
    camera->someBitFlags &= 0xFFFB;
    check_bounding_collision(&camera->collision, temp, sp84, sp80, sp7C);

    camera->pos[0] = sp84;
    camera->pos[1] = sp80;
    camera->pos[2] = sp7C;

    camera->lookAt[0] = sp64[0];
    camera->lookAt[1] = sp64[1];
    camera->lookAt[2] = sp64[2];

    temp_f12 = camera->lookAt[0] - camera->pos[0];
    sp90 = camera->lookAt[1] - camera->pos[1];
    temp_f14 = camera->lookAt[2] - camera->pos[2];

    camera->rot[1] = atan2s(temp_f12, temp_f14);
    camera->rot[0] = atan2s(sqrtf((temp_f12 * temp_f12) + (temp_f14 * temp_f14)), sp90);
    camera->rot[2] = 0;
}

void func_8001EE98(Player* player, Camera* camera, s8 index) {
    s32 cameraIndex = camera->cameraId;

    update_first_person_view_toggle(index);

    switch (gModeSelection) {
        case GRAND_PRIX:
            // clang-format off
            if (((player->type & PLAYER_CINEMATIC_MODE) == PLAYER_CINEMATIC_MODE) || (gDemoMode == 1)) { camera->mode = 3;
            //             -->                 -->        Scroll right        -->      bit more     -->     ^ Required for matching
                // clang-format on
            } else if (gIsGamePaused == 1) {
                func_8001A0A4(&camera->mode, camera, player, index, cameraIndex);
            } else {
                func_8001A0DC(&camera->mode, camera, player, index, cameraIndex);
            }
            break;
        case BATTLE:
            if ((gDemoMode == 1) || ((D_8015F894 == 2) && (D_80164A89 == 1))) {
                if (D_80164A88 == 0) {
                    func_80019ED0();
                }
                D_80164A88 = 1;
                camera->mode = 3;
            } else {
                D_80164A88 = 0;
                if (gIsGamePaused == 1) {
                    func_8001A0A4(&camera->mode, camera, player, index, cameraIndex);
                } else {
                    func_8001A0DC(&camera->mode, camera, player, index, cameraIndex);
                }
                camera->mode = 9;
            }
            break;
        case TIME_TRIALS:
            if (((gPlayerOne->type & PLAYER_CINEMATIC_MODE) == PLAYER_CINEMATIC_MODE) || (gDemoMode == 1)) {
                camera->mode = 3;
            } else {
                if (gIsGamePaused == 1) {
                    func_8001A0A4(&camera->mode, camera, player, index, cameraIndex);
                } else {
                    func_8001A0DC(&camera->mode, camera, player, index, cameraIndex);
                }
                camera->mode = 1;
            }
            break;
        case VERSUS:
            if (((player->type & PLAYER_CINEMATIC_MODE) == PLAYER_CINEMATIC_MODE) || (gDemoMode == 1) ||
                (D_8015F894 == 2)) {
                camera->mode = 3;
            } else {
                if (gIsGamePaused == 1) {
                    func_8001A0A4(&camera->mode, camera, player, index, cameraIndex);
                } else {
                    func_8001A0DC(&camera->mode, camera, player, index, cameraIndex);
                }
                camera->mode = 1;
            }
            break;
    }
    if (gIsGamePaused == 0) {
        switch (camera->mode) {
            case 3: // end of race
                func_8001A588(&camera->mode, camera, player, index, cameraIndex);
                break;
            case 1: // player camera
                if (((player->lakituProps & LAKITU_RETRIEVAL) == LAKITU_RETRIEVAL) || ((player->lakituProps & HELD_BY_LAKITU) == HELD_BY_LAKITU)) {
                    func_8001E8E8(camera, player, index);
                    break;
                }
                func_8001E45C(camera, player, index);
                break;
            case 8: // Transition start
                func_8001E0C4(camera, player, index);
                func_8001F87C(cameraIndex);
                break;
            case 9:
                if (((player->lakituProps & LAKITU_RETRIEVAL) == LAKITU_RETRIEVAL) || ((player->lakituProps & HELD_BY_LAKITU) == HELD_BY_LAKITU)) {
                    func_8001E8E8(camera, player, index);
                    break;
                }
                func_8001EA0C(camera, player, index);
                break;
        }
    }

    if ((gIsGamePaused == 0) && CM_IsFirstPersonViewEnabled(index) && (camera->mode != 3) && (gDemoMode != 1) &&
        ((player->type & PLAYER_CINEMATIC_MODE) != PLAYER_CINEMATIC_MODE)) {
        apply_first_person_view_camera(camera, player, index);
    }
}

static f32 camera_get_speed_zoom_fov(Camera* camera, Player* player, s32 playerIndex) {
    f32 speedRatio = 0.0f;
    f32 speedForRatio = 0.0f;
    f32 speedReference = 1.0f;
    f32 boostAmount;
    f32 shakeAmount = 0.0f;
    f32 roadRoughness = 0.0f;
    f32 driftAmount = 0.0f;
    f32 boostShakeAmount = 0.0f;
    f32 postFxScale;
    f32 speedCurve;
    f32 wideFov;
    f32 narrowFov;
    s32 postFxFinished;
    f32 targetFov;
    f32 currentFov = camera->fieldOfView;
    const f32 zoomStep = 1.0f;
    char cvarName[96];

    if (!is_arcadekart_race_speed_fx_active()) {
        if ((playerIndex >= 0) && (playerIndex < NUM_PLAYERS)) {
            sArcadeKartPostFxPlayerScale[playerIndex] = 0.0f;
        }
    }

    speedCurve = get_arcadekart_camera_speed_curve(player, &speedForRatio, &speedReference, &speedRatio);
    boostAmount = is_arcadekart_race_speed_fx_active() ? (D_80164498[playerIndex] / 25.0f) : 0.0f;
    if (boostAmount < 0.0f) {
        boostAmount = 0.0f;
    }
    if (boostAmount > 1.0f) {
        boostAmount = 1.0f;
    }

    if ((player != NULL) && is_arcadekart_race_speed_fx_active()) {
        if ((player->effects & (HIT_BY_ITEM_EFFECT | HIT_EFFECT | LIGHTNING_EFFECT)) != 0) {
            shakeAmount = 1.0f;
        }
        roadRoughness = get_arcadekart_surface_roughness_normalized(player);
        driftAmount = ((player->effects & DRIFTING_EFFECT) == DRIFTING_EFFECT) ? 1.0f : 0.0f;
        boostShakeAmount = is_arcadekart_player_boosting(player) ? 1.0f : 0.0f;
    }

    postFxFinished = (player == NULL) || !is_arcadekart_race_speed_fx_active() ||
                     (playerHUD[playerIndex].lapCount >= 3) || (gRaceState == RACE_FINISHED) ||
                     (gRaceState == RACE_EXIT) || (gGamestate != RACING);
    postFxScale = step_arcadekart_postfx_scale(playerIndex, postFxFinished);
    wideFov = CVarGetFloat("gArcadeKart.Camera.SpeedWideFov", 100.0f);
    narrowFov = CVarGetFloat("gArcadeKart.Camera.SpeedNarrowFov", 60.0f);

    CVarSetInteger("gArcadeKart.PostFx.ScreenMode", gActiveScreenMode);
    snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.SpeedRatio", playerIndex + 1);
    CVarSetFloat(cvarName, speedRatio * postFxScale);
    snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.BoostAmount", playerIndex + 1);
    CVarSetFloat(cvarName, boostAmount * postFxScale);
    snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.ShakeAmount", playerIndex + 1);
    CVarSetFloat(cvarName, shakeAmount * postFxScale);
    snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.RoadRoughness", playerIndex + 1);
    CVarSetFloat(cvarName, roadRoughness * postFxScale);
    snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.DriftAmount", playerIndex + 1);
    CVarSetFloat(cvarName, driftAmount * postFxScale);
    snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.BoostShakeAmount", playerIndex + 1);
    CVarSetFloat(cvarName, boostShakeAmount * postFxScale);
    snprintf(cvarName, sizeof(cvarName), "gArcadeKart.PostFx.Player%d.FxScale", playerIndex + 1);
    CVarSetFloat(cvarName, postFxScale);

    targetFov = wideFov + ((narrowFov - wideFov) * speedCurve);
    CVarSetFloat("gArcadeKart.Camera.DebugSpeedForRatio", speedForRatio);
    CVarSetFloat("gArcadeKart.Camera.DebugSpeedReference", speedReference);
    CVarSetFloat("gArcadeKart.Camera.DebugSpeedRatio", speedRatio);
    CVarSetFloat("gArcadeKart.Camera.DebugSpeedCurve", speedCurve);
    CVarSetFloat("gArcadeKart.Camera.DebugTargetFov", targetFov);
    if (currentFov < targetFov) {
        currentFov += zoomStep;
        if (currentFov > targetFov) {
            currentFov = targetFov;
        }
    } else if (currentFov > targetFov) {
        currentFov -= zoomStep;
        if (currentFov < targetFov) {
            currentFov = targetFov;
        }
    }
    return currentFov;
}

void func_8001F394(Player* player) {
    f32 var_f0;
    UNUSED s32 pad;
    s32 playerIndex = player - gPlayerOne;
    UNUSED s32 pad2;

    Camera* camera = CM_GetPlayerCamera(playerIndex);

    if (NULL == camera) {
        printf("[camera.c][func_8001F394] Could not find a camera using GetPlayerCamera()\n");
        return;
    }
    var_f0 = camera->fieldOfView;

    if (player == gPlayerOne) {
        playerIndex = 0;
    }
    if (player == gPlayerTwo) {
        playerIndex = 1;
    }
    if (player == gPlayerThree) {
        playerIndex = 2;
    }
    if (player == gPlayerFour) {
        playerIndex = 3;
    }

    if (D_80164A08[playerIndex] == 0) {
        if (player->triggers & DRAG_ITEM_EFFECT) {
            D_80164A08[playerIndex] = 1;
        }
        if ((player->effects & BOOST_EFFECT) == BOOST_EFFECT) {
            D_80164A08[playerIndex] = 2;
        }
        if ((player->effects & BOOST_RAMP_ASPHALT_EFFECT) == BOOST_RAMP_ASPHALT_EFFECT) {
            D_80164A08[playerIndex] = 3;
        }
        if ((player->triggers & THWOMP_SQUISH_TRIGGER) == THWOMP_SQUISH_TRIGGER) {
            D_80164A08[playerIndex] = 4;
        }
        if (((player->effects & 0x80) == 0x80) || ((player->effects & 0x40) == 0x40)) {
            D_80164A08[playerIndex] = 5;
        }
        D_80164498[playerIndex] = 0.0f;
    }
    switch (D_80164A08[playerIndex]) {
        case 1:
            if (player->triggers & DRAG_ITEM_EFFECT) {
                move_f32_towards(&D_80164498[playerIndex], 20.0f, 0.2f);
            } else {
                if (D_80164498[playerIndex] > 1.0f) {
                    D_80164498[playerIndex] -= 1.0f;
                } else {
                    D_80164A08[playerIndex] = 0;
                    D_80164498[playerIndex] = 0.0f;
                }
            }
            break;
        case 2:
            if ((player->effects & BOOST_EFFECT) == BOOST_EFFECT) {
                if (player->boostTimer != 0) {
                    move_f32_towards(&D_80164498[playerIndex], 8.0f, 0.2f);
                }
            } else {
                if (D_80164498[playerIndex] > 1.0f) {
                    D_80164498[playerIndex] -= 2.0f;
                } else {
                    D_80164A08[playerIndex] = 0;
                    D_80164498[playerIndex] = 0.0f;
                }
            }
            break;
        case 3:
            if (((player->effects & BOOST_RAMP_ASPHALT_EFFECT) == BOOST_RAMP_ASPHALT_EFFECT) &&
                ((player->effects & 8) == 8)) {
                move_f32_towards(&D_80164498[playerIndex], 20.0f, 0.1f);
            } else {
                if (D_80164498[playerIndex] > 1.0f) {
                    D_80164498[playerIndex] -= 1.0f;
                } else {
                    D_80164A08[playerIndex] = 0;
                    D_80164498[playerIndex] = 0.0f;
                }
            }
            break;
        case 4:
            if ((player->triggers & THWOMP_SQUISH_TRIGGER) == THWOMP_SQUISH_TRIGGER) {
                move_f32_towards(&D_80164498[playerIndex], 25.0f, 1.0f);
            } else {
                if (D_80164498[playerIndex] > 1.0f) {
                    D_80164498[playerIndex] -= 2.0f;
                } else {
                    D_80164A08[playerIndex] = 0;
                    D_80164498[playerIndex] = 0.0f;
                }
            }
            break;
        case 5:
            if (((player->effects & 0x80) == 0x80) || ((player->effects & 0x40) == 0x40)) {
                move_f32_towards(&D_80164498[playerIndex], 18.0f, 0.2f);
            } else {
                if (D_80164498[playerIndex] > 1.0f) {
                    D_80164498[playerIndex] -= 2.0f;
                } else {
                    D_80164A08[playerIndex] = 0;
                    D_80164498[playerIndex] = 0.0f;
                }
            }
            break;
    }
    switch (gActiveScreenMode) {
        case SCREEN_MODE_1P:
            if (D_80164A28 == 1) {
                D_80164498[playerIndex] = 40.0f;
            }
            if (D_80164A28 == 2) {
                if (D_80164498[playerIndex] >= 0.0f) {
                    D_80164498[playerIndex] -= 0.8;
                }
                if (D_80164498[playerIndex] <= 0.0f) {
                    D_80164A28 = 0;
                    D_80164498[playerIndex] = 0.0f;
                }
            }
            var_f0 = camera_get_speed_zoom_fov(camera, player, playerIndex);
            break;
        case SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL:
        case SCREEN_MODE_2P_SPLITSCREEN_VERTICAL:
        case SCREEN_MODE_3P_4P_SPLITSCREEN:
            var_f0 = camera_get_speed_zoom_fov(camera, player, playerIndex);
            break;
    }
    camera->fieldOfView = var_f0;
    camera->unk_B4 = var_f0;
}

void func_8001F87C(s32 cameraId) {
    if (gActiveScreenMode == SCREEN_MODE_1P) {
        if (gModeSelection == GRAND_PRIX) {
            for (size_t i = 0; i < NUM_PLAYERS; i++) {
                if ((gPlayerOne[i].type & PLAYER_STAGING) || (gPlayerOne[i].type & PLAYER_UNKNOWN_0x80)) {
                    break;
                }
                if (i == PLAYER_EIGHT) {
                    sStagingTimer[cameraId] += 1;
                }
                if ((i == PLAYER_EIGHT) && (sStagingTimer[cameraId] == 60)) {
                    D_80164A28 = 2;
                    cameras[cameraId].mode = 1;
                    cameras[cameraId].rot[1] = gPlayerOne[i].rotation[1];
                    cameras[cameraId].unk_2C = gPlayerOne[i].rotation[1];
                }
            }
        }
    }
}
