#include <libultraship.h>
#include <libultra/gbi.h>
#include <libc/math.h>

#include "camera.h"
#include "defines.h"
#include "first_person_kart.h"
#include "main.h"
#include "racing/math_util.h"
#include "render_player.h"

#include "first_person_kart_mesh.inc.c"
#include "first_person_kart_wheels.inc.c"

#define FIRST_PERSON_KART_MESH_YAW_OFFSET 0x4000
#define FIRST_PERSON_KART_WHEEL_STEER_MAX 0x1000
#define FIRST_PERSON_KART_VECTOR_EPSILON 0.0001f

#define FIRST_PERSON_KART_COLOR_RECOLOR_R 0xD8
#define FIRST_PERSON_KART_COLOR_RECOLOR_G 0x08
#define FIRST_PERSON_KART_COLOR_RECOLOR_B 0x05
#define FIRST_PERSON_KART_COLOR_SILVER_R 0xB8
#define FIRST_PERSON_KART_COLOR_SILVER_G 0xB8
#define FIRST_PERSON_KART_COLOR_SILVER_B 0xAE
#define FIRST_PERSON_KART_COLOR_DARK_R 0x14
#define FIRST_PERSON_KART_COLOR_DARK_G 0x14
#define FIRST_PERSON_KART_COLOR_DARK_B 0x16
#define FIRST_PERSON_KART_COLOR_BLUE_R 0x30
#define FIRST_PERSON_KART_COLOR_BLUE_G 0x54
#define FIRST_PERSON_KART_COLOR_BLUE_B 0xD0
#define FIRST_PERSON_KART_COLOR_HUB_R 0xFF
#define FIRST_PERSON_KART_COLOR_HUB_G 0xC6
#define FIRST_PERSON_KART_COLOR_HUB_B 0x10

static void render_first_person_kart_primitive_dl(Gfx* dl, u8 red, u8 green, u8 blue) {
    gDPSetPrimColor(gDisplayListHead++, 0, 0, red, green, blue, 0xFF);
    gSPDisplayList(gDisplayListHead++, dl);
}

static void set_first_person_kart_basis_mtx(Mat4 dest, const Vec3f pos, const Vec3f right, const Vec3f up,
                                            const Vec3f forward) {
    dest[0][0] = right[0];
    dest[0][1] = right[1];
    dest[0][2] = right[2];
    dest[0][3] = 0.0f;
    dest[1][0] = up[0];
    dest[1][1] = up[1];
    dest[1][2] = up[2];
    dest[1][3] = 0.0f;
    dest[2][0] = forward[0];
    dest[2][1] = forward[1];
    dest[2][2] = forward[2];
    dest[2][3] = 0.0f;
    dest[3][0] = pos[0];
    dest[3][1] = pos[1];
    dest[3][2] = pos[2];
    dest[3][3] = 1.0f;
}

static s16 get_first_person_kart_steering_angle(s32 screenIdx) {
    s32 stickX = gControllers[screenIdx].rawStickX;

    if (stickX < -80) {
        stickX = -80;
    }
    if (stickX > 80) {
        stickX = 80;
    }
    return (s16) ((-stickX * FIRST_PERSON_KART_WHEEL_STEER_MAX) / 80);
}

static void set_first_person_kart_visual_tyre_pos(Vec3f dest, const Player* player, s32 tyreIndex, s16 bodyYaw) {
    const Vec3f* source = &player->tyres[tyreIndex].pos;

    (void) bodyYaw;
    dest[0] = (*source)[0];
    dest[1] = (*source)[1];
    dest[2] = (*source)[2];
}

static void render_first_person_kart_wheel_mesh(const Vec3f pos, const Vec3f bodyRight, const Vec3f bodyUp,
                                                const Vec3f bodyForward, s16 steerYaw, Gfx* tireDl, Gfx* hubDl) {
    Mat4 wheelMtx;
    Vec3f wheelRight;
    Vec3f wheelForward;

    wheelForward[0] = (bodyForward[0] * coss(steerYaw)) + (bodyRight[0] * sins(steerYaw));
    wheelForward[1] = (bodyForward[1] * coss(steerYaw)) + (bodyRight[1] * sins(steerYaw));
    wheelForward[2] = (bodyForward[2] * coss(steerYaw)) + (bodyRight[2] * sins(steerYaw));
    wheelRight[0] = (bodyRight[0] * coss(steerYaw)) - (bodyForward[0] * sins(steerYaw));
    wheelRight[1] = (bodyRight[1] * coss(steerYaw)) - (bodyForward[1] * sins(steerYaw));
    wheelRight[2] = (bodyRight[2] * coss(steerYaw)) - (bodyForward[2] * sins(steerYaw));

    set_first_person_kart_basis_mtx(wheelMtx, pos, wheelRight, bodyUp, wheelForward);
    mtxf_scale(wheelMtx, FP_MESHY_KART_WHEEL_SCALE);
    if (render_set_position(wheelMtx, 0) != 0) {
        render_first_person_kart_primitive_dl(tireDl, FIRST_PERSON_KART_COLOR_DARK_R, FIRST_PERSON_KART_COLOR_DARK_G,
                                              FIRST_PERSON_KART_COLOR_DARK_B);
        render_first_person_kart_primitive_dl(hubDl, FIRST_PERSON_KART_COLOR_HUB_R, FIRST_PERSON_KART_COLOR_HUB_G,
                                              FIRST_PERSON_KART_COLOR_HUB_B);
    }
}

static void render_first_person_kart_wheels(Player* player, s16 bodyYaw, s32 screenIdx, const Vec3f bodyRight,
                                            const Vec3f bodyUp, const Vec3f bodyForward) {
    s16 steerYaw = get_first_person_kart_steering_angle(screenIdx);
    Vec3f frontLeft;
    Vec3f frontRight;
    Vec3f backLeft;
    Vec3f backRight;

    set_first_person_kart_visual_tyre_pos(frontLeft, player, FRONT_LEFT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(frontRight, player, FRONT_RIGHT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(backLeft, player, BACK_LEFT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(backRight, player, BACK_RIGHT, bodyYaw);

    render_first_person_kart_wheel_mesh(frontLeft, bodyRight, bodyUp, bodyForward, steerYaw,
                                        sFirstPersonKartFrontWheelTireDl, sFirstPersonKartFrontWheelHubDl);
    render_first_person_kart_wheel_mesh(frontRight, bodyRight, bodyUp, bodyForward, steerYaw,
                                        sFirstPersonKartFrontWheelTireDl, sFirstPersonKartFrontWheelHubDl);
    render_first_person_kart_wheel_mesh(backLeft, bodyRight, bodyUp, bodyForward, 0, sFirstPersonKartRearWheelTireDl,
                                        sFirstPersonKartRearWheelHubDl);
    render_first_person_kart_wheel_mesh(backRight, bodyRight, bodyUp, bodyForward, 0, sFirstPersonKartRearWheelTireDl,
                                        sFirstPersonKartRearWheelHubDl);
}

static f32 first_person_kart_vec3f_dot(const Vec3f a, const Vec3f b) {
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

static void first_person_kart_vec3f_cross(Vec3f dest, const Vec3f a, const Vec3f b) {
    dest[0] = (a[1] * b[2]) - (a[2] * b[1]);
    dest[1] = (a[2] * b[0]) - (a[0] * b[2]);
    dest[2] = (a[0] * b[1]) - (a[1] * b[0]);
}

static s32 first_person_kart_vec3f_normalize(Vec3f vec) {
    f32 length = sqrtf(first_person_kart_vec3f_dot(vec, vec));

    if (length < FIRST_PERSON_KART_VECTOR_EPSILON) {
        return 0;
    }

    vec[0] /= length;
    vec[1] /= length;
    vec[2] /= length;
    return 1;
}

static void first_person_kart_add_tyre_plane_normal(Vec3f normalSum, const Vec3f a, const Vec3f b, const Vec3f c) {
    Vec3f ab;
    Vec3f ac;
    Vec3f normal;

    ab[0] = b[0] - a[0];
    ab[1] = b[1] - a[1];
    ab[2] = b[2] - a[2];
    ac[0] = c[0] - a[0];
    ac[1] = c[1] - a[1];
    ac[2] = c[2] - a[2];

    first_person_kart_vec3f_cross(normal, ab, ac);
    if (normal[1] < 0.0f) {
        normal[0] = -normal[0];
        normal[1] = -normal[1];
        normal[2] = -normal[2];
    }

    if (first_person_kart_vec3f_normalize(normal) != 0) {
        normalSum[0] += normal[0];
        normalSum[1] += normal[1];
        normalSum[2] += normal[2];
    }
}

static void set_first_person_kart_up_from_tyres(Vec3f up, const Player* player, s16 bodyYaw) {
    Vec3f frontLeft;
    Vec3f frontRight;
    Vec3f backLeft;
    Vec3f backRight;

    set_first_person_kart_visual_tyre_pos(frontLeft, player, FRONT_LEFT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(frontRight, player, FRONT_RIGHT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(backLeft, player, BACK_LEFT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(backRight, player, BACK_RIGHT, bodyYaw);

    up[0] = 0.0f;
    up[1] = 0.0f;
    up[2] = 0.0f;

    first_person_kart_add_tyre_plane_normal(up, frontLeft, frontRight, backLeft);
    first_person_kart_add_tyre_plane_normal(up, frontLeft, frontRight, backRight);
    first_person_kart_add_tyre_plane_normal(up, frontLeft, backLeft, backRight);
    first_person_kart_add_tyre_plane_normal(up, frontRight, backLeft, backRight);

    if (first_person_kart_vec3f_normalize(up) == 0) {
        up[0] = 0.0f;
        up[1] = 1.0f;
        up[2] = 0.0f;
    }
}

static void first_person_kart_add_normalized_delta(Vec3f dest, const Vec3f from, const Vec3f to) {
    Vec3f delta;

    delta[0] = to[0] - from[0];
    delta[1] = to[1] - from[1];
    delta[2] = to[2] - from[2];

    if (first_person_kart_vec3f_normalize(delta) != 0) {
        dest[0] += delta[0];
        dest[1] += delta[1];
        dest[2] += delta[2];
    }
}

static void set_first_person_kart_forward_from_tyres(Vec3f forward, const Player* player, s16 bodyYaw) {
    Vec3f frontLeft;
    Vec3f frontRight;
    Vec3f backLeft;
    Vec3f backRight;

    set_first_person_kart_visual_tyre_pos(frontLeft, player, FRONT_LEFT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(frontRight, player, FRONT_RIGHT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(backLeft, player, BACK_LEFT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(backRight, player, BACK_RIGHT, bodyYaw);

    forward[0] = 0.0f;
    forward[1] = 0.0f;
    forward[2] = 0.0f;

    first_person_kart_add_normalized_delta(forward, backLeft, frontLeft);
    first_person_kart_add_normalized_delta(forward, backRight, frontRight);

    if (first_person_kart_vec3f_normalize(forward) == 0) {
        forward[0] = sins(bodyYaw);
        forward[1] = 0.0f;
        forward[2] = coss(bodyYaw);
    }
}

static void set_first_person_kart_body_pos_from_tyres(Vec3f dest, const Player* player, s16 bodyYaw) {
    Vec3f frontLeft;
    Vec3f frontRight;
    Vec3f backLeft;
    Vec3f backRight;

    set_first_person_kart_visual_tyre_pos(frontLeft, player, FRONT_LEFT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(frontRight, player, FRONT_RIGHT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(backLeft, player, BACK_LEFT, bodyYaw);
    set_first_person_kart_visual_tyre_pos(backRight, player, BACK_RIGHT, bodyYaw);

    dest[0] = (frontLeft[0] + frontRight[0] + backLeft[0] + backRight[0]) * 0.25f;
    dest[1] = (frontLeft[1] + frontRight[1] + backLeft[1] + backRight[1]) * 0.25f;
    dest[2] = (frontLeft[2] + frontRight[2] + backLeft[2] + backRight[2]) * 0.25f;
}

static void set_first_person_kart_orientation_from_tyres(Vec3f pos, Vec3f right, Vec3f up, Vec3f forward,
                                                         const Player* player, s16 bodyYaw) {
    set_first_person_kart_body_pos_from_tyres(pos, player, bodyYaw);
    set_first_person_kart_up_from_tyres(up, player, bodyYaw);
    set_first_person_kart_forward_from_tyres(forward, player, bodyYaw);

    first_person_kart_vec3f_cross(right, up, forward);
    if (first_person_kart_vec3f_normalize(right) == 0) {
        right[0] = coss(bodyYaw);
        right[1] = 0.0f;
        right[2] = -sins(bodyYaw);
    }

    first_person_kart_vec3f_cross(forward, right, up);
    if (first_person_kart_vec3f_normalize(forward) == 0) {
        forward[0] = sins(bodyYaw);
        forward[1] = 0.0f;
        forward[2] = coss(bodyYaw);
    }
}

s32 get_first_person_kart_body_pose(FirstPersonKartBodyPose* pose, const Player* player) {
    if ((pose == NULL) || (player == NULL)) {
        return 0;
    }

    pose->bodyYaw = -player->rotation[1] + FIRST_PERSON_KART_MESH_YAW_OFFSET;
    set_first_person_kart_orientation_from_tyres(pose->pos, pose->right, pose->up, pose->forward, player,
                                                 pose->bodyYaw);
    return 1;
}

static void mtxf_first_person_kart_body_from_orientation(Mat4 dest, const Vec3f pos, const Vec3f right, const Vec3f up,
                                                         const Vec3f forward) {
    Vec3f meshRight;
    Vec3f meshForward;

    meshRight[0] = right[0];
    meshRight[1] = right[1];
    meshRight[2] = right[2];
    meshForward[0] = forward[0];
    meshForward[1] = forward[1];
    meshForward[2] = forward[2];

    set_first_person_kart_basis_mtx(dest, pos, meshRight, up, meshForward);
}

void render_first_person_kart(Camera* camera, s32 screenIdx) {
    Player* player;
    FirstPersonKartBodyPose pose;
    Mat4 meshMtx;

    if ((camera == NULL) || (screenIdx < 0) || (screenIdx >= 4) || !CM_IsFirstPersonViewEnabled(screenIdx) ||
        (gDemoMode == 1)) {
        return;
    }

    player = &gPlayers[screenIdx];
    if (((player->type & PLAYER_EXISTS) == 0) || ((player->type & PLAYER_HUMAN) == 0) ||
        ((player->type & PLAYER_INVISIBLE_OR_BOMB) != 0) || ((player->type & PLAYER_CINEMATIC_MODE) != 0)) {
        return;
    }

    if (get_first_person_kart_body_pose(&pose, player) == 0) {
        return;
    }

    gSPTexture(gDisplayListHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode(gDisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING | G_CULL_BOTH);
    gSPSetGeometryMode(gDisplayListHead++, G_ZBUFFER | G_SHADE);

    mtxf_first_person_kart_body_from_orientation(meshMtx, pose.pos, pose.right, pose.up, pose.forward);
    mtxf_scale(meshMtx, FP_MESHY_KART_BODY_SCALE);
    if (render_set_position(meshMtx, 0) != 0) {
        render_first_person_kart_primitive_dl(sMeshyKartBodyDarkDl, FIRST_PERSON_KART_COLOR_DARK_R,
                                              FIRST_PERSON_KART_COLOR_DARK_G, FIRST_PERSON_KART_COLOR_DARK_B);
        render_first_person_kart_primitive_dl(sMeshyKartBodySilverDl, FIRST_PERSON_KART_COLOR_SILVER_R,
                                              FIRST_PERSON_KART_COLOR_SILVER_G, FIRST_PERSON_KART_COLOR_SILVER_B);
        render_first_person_kart_primitive_dl(sMeshyKartBodyBlueDl, FIRST_PERSON_KART_COLOR_BLUE_R,
                                              FIRST_PERSON_KART_COLOR_BLUE_G, FIRST_PERSON_KART_COLOR_BLUE_B);
        render_first_person_kart_primitive_dl(sMeshyKartBodyRecolorDl, FIRST_PERSON_KART_COLOR_RECOLOR_R,
                                              FIRST_PERSON_KART_COLOR_RECOLOR_G, FIRST_PERSON_KART_COLOR_RECOLOR_B);
    }

    render_first_person_kart_wheels(player, pose.bodyYaw, screenIdx, pose.right, pose.up, pose.forward);
}
