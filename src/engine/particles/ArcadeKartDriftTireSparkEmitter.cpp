#include "ArcadeKartDriftTireSparkEmitter.h"

#include <cstdint>

#include "port/interpolation/FrameInterpolation.h"

extern "C" {
#include "assets/models/common_data.h"
#include "assets/textures/common_data.h"
#include "camera.h"
#include "code_80057C60.h"
#include "defines.h"
#include "macros.h"
#include "main.h"
#include "math_util.h"
#include "math_util_2.h"
#include "render_objects.h"
#include "render_player.h"
}

#define ARCADEKART_DRIFT_TIRE_SPARK_MAX_PARTICLES 24
#define ARCADEKART_DRIFT_TIRE_SPARK_SPAWN_RATE 45.0f
#define ARCADEKART_DRIFT_TIRE_SPARK_LIFETIME_FRAMES 4.0f
#define ARCADEKART_DRIFT_TIRE_SPARK_INITIAL_SCALE 0.95f
#define ARCADEKART_DRIFT_TIRE_SPARK_FINAL_SCALE 0.55f
#define ARCADEKART_DRIFT_TIRE_SPARK_RANDOM_SCALE_MIN 0.5f
#define ARCADEKART_DRIFT_TIRE_SPARK_RANDOM_SCALE_MAX 1.0f
#define ARCADEKART_DRIFT_TIRE_SPARK_ALPHA_POWER 2.8f
#define ARCADEKART_DRIFT_TIRE_SPARK_HEIGHT_OFFSET 1.85f
#define ARCADEKART_DRIFT_TIRE_ZAP_CORE_SCALE 1.326f
#define ARCADEKART_DRIFT_TIRE_ZAP_JITTER_RADIUS 0.34f
#define ARCADEKART_DRIFT_TIRE_SPARK_DRAW_SCALE 2.5f
#define ARCADEKART_DRIFT_TIRE_SPARK_SIDE_MOTION 1.35f
#define ARCADEKART_DRIFT_TIRE_SPARK_LIFT_MOTION 1.45f
#define ARCADEKART_DRIFT_TIRE_SPARK_BACK_MOTION 2.4f
#define ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_VELOCITY_X_RANGE 0.52f
#define ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_VELOCITY_Y_MIN 0.38f
#define ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_VELOCITY_Y_MAX 0.92f
#define ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_BACKWARD_VELOCITY_MIN 0.78f
#define ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_BACKWARD_VELOCITY_MAX 1.45f

static Vtx sArcadeKartDriftTireZapCoreLeftVtx[] = {
    { { { -2, 2, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 0, 2, 0 }, 0, { 1984, 0 }, { 255, 255, 255, 255 } } },
    { { { 0, -2, 0 }, 0, { 1984, 4032 }, { 255, 255, 255, 255 } } },
    { { { -2, -2, 0 }, 0, { 0, 4032 }, { 255, 255, 255, 255 } } },
};

static Vtx sArcadeKartDriftTireZapCoreRightVtx[] = {
    { { { 0, 2, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 2, 2, 0 }, 0, { 1984, 0 }, { 255, 255, 255, 255 } } },
    { { { 2, -2, 0 }, 0, { 1984, 4032 }, { 255, 255, 255, 255 } } },
    { { { 0, -2, 0 }, 0, { 0, 4032 }, { 255, 255, 255, 255 } } },
};

static Vtx sArcadeKartDriftTireZapCoreLeftFlippedVtx[] = {
    { { { -2, 2, 0 }, 0, { 1984, 0 }, { 255, 255, 255, 255 } } },
    { { { 0, 2, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 0, -2, 0 }, 0, { 0, 4032 }, { 255, 255, 255, 255 } } },
    { { { -2, -2, 0 }, 0, { 1984, 4032 }, { 255, 255, 255, 255 } } },
};

static Vtx sArcadeKartDriftTireZapCoreRightFlippedVtx[] = {
    { { { 0, 2, 0 }, 0, { 1984, 0 }, { 255, 255, 255, 255 } } },
    { { { 2, 2, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 2, -2, 0 }, 0, { 0, 4032 }, { 255, 255, 255, 255 } } },
    { { { 0, -2, 0 }, 0, { 1984, 4032 }, { 255, 255, 255, 255 } } },
};

static f32 random_unit(void) {
    return (f32) random_int(10001U) / 10000.0f;
}

static f32 random_centered_unit(void) {
    return ((f32) random_int(2001U) / 1000.0f) - 1.0f;
}

static f32 random_range(f32 min, f32 max) {
    return min + ((max - min) * random_unit());
}

static void offset_point_by_local_motion(const Vec3f origin, s16 yaw, f32 localX, f32 localY, f32 localZ, Vec3f out) {
    out[0] = origin[0] + (localX * coss(yaw)) + (localZ * sins(yaw));
    out[1] = origin[1] + localY;
    out[2] = origin[2] + (localZ * coss(yaw)) - (localX * sins(yaw));
}

static s32 get_rainbow_tint(void) {
    static const s32 colors[] = {
        0xFF4040, 0xFF9A20, 0xFFE640, 0x40FF5C, 0x40D6FF, 0x7060FF, 0xFF58E8,
    };

    return colors[(gGlobalTimer / 2) % (sizeof(colors) / sizeof(colors[0]))];
}

static void get_stage_tint(s8 stage, s16* red, s16* green, s16* blue) {
    s32 rgb;

    switch (stage) {
        case ARCADEKART_DRIFT_TIRE_SPARK_STAGE_SLIDE:
            rgb = 0xFFE640;
            break;
        case ARCADEKART_DRIFT_TIRE_SPARK_STAGE_TURBO:
            rgb = get_rainbow_tint();
            break;
        case ARCADEKART_DRIFT_TIRE_SPARK_STAGE_DRIFT:
        default:
            rgb = 0xFF8C00;
            break;
    }

    *red = (rgb >> 16) & 0xFF;
    *green = (rgb >> 8) & 0xFF;
    *blue = rgb & 0xFF;
}

static ArcadeKartParticleParams make_drift_tire_spark_params(void) {
    ArcadeKartParticleParams params;

    params.Space = ArcadeKartParticleSpace::Local;
    params.Active = false;
    params.Looping = true;
    params.MaxParticles = ARCADEKART_DRIFT_TIRE_SPARK_MAX_PARTICLES;
    params.SpawnRatePerSecond = ARCADEKART_DRIFT_TIRE_SPARK_SPAWN_RATE;
    params.LifetimeFrames = ARCADEKART_DRIFT_TIRE_SPARK_LIFETIME_FRAMES;
    params.InitialVelocity[0] = 0.0f;
    params.InitialVelocity[1] = 0.0f;
    params.InitialVelocity[2] = 0.0f;
    params.InitialScale = ARCADEKART_DRIFT_TIRE_SPARK_INITIAL_SCALE;
    params.FinalScale = ARCADEKART_DRIFT_TIRE_SPARK_FINAL_SCALE;
    params.RandomScaleMin = ARCADEKART_DRIFT_TIRE_SPARK_RANDOM_SCALE_MIN;
    params.RandomScaleMax = ARCADEKART_DRIFT_TIRE_SPARK_RANDOM_SCALE_MAX;
    params.InitialRed = 0xFF;
    params.InitialGreen = 0xB8;
    params.InitialBlue = 0x24;
    params.FinalRed = 0xFF;
    params.FinalGreen = 0x54;
    params.FinalBlue = 0x10;
    params.ColorPower = 1.0f;
    params.ColorOverLife = true;
    params.InitialAlpha = 0xFF;
    params.FinalAlpha = 0;
    params.AlphaPower = ARCADEKART_DRIFT_TIRE_SPARK_ALPHA_POWER;
    params.TextureIndex = 0;
    params.TextureCount = 4;
    params.RandomTexture = true;
    params.RandomHorizontalFlip = true;
    params.RandomBillboardRoll = false;
    params.InitialBillboardRoll = 0;
    return params;
}

ArcadeKartDriftTireSparkEmitter::ArcadeKartDriftTireSparkEmitter(Player* player, s8 playerId, s8 tireIndex)
    : ArcadeKartParticleEmitter(make_drift_tire_spark_params()), PlayerId(playerId), TireIndex(tireIndex) {
    AttachToPlayer(player);
}

void ArcadeKartDriftTireSparkEmitter::ConfigureForPlayer(Player* player, bool active, s8 driftStage) {
    AttachToPlayer(player);
    DriftStage = active ? driftStage : ARCADEKART_DRIFT_TIRE_SPARK_STAGE_NONE;
    SetLocalOffset(0.0f, 0.0f, 0.0f);
    SetActive(active);
}

bool ArcadeKartDriftTireSparkEmitter::DrawsInWorldParticlePass() {
    return false;
}

void ArcadeKartDriftTireSparkEmitter::GetLocationAndRotation(Vec3f location, Vec3s rotation) const {
    Player* player = GetAttachedPlayer();

    if (player == nullptr) {
        ArcadeKartParticleEmitter::GetLocationAndRotation(location, rotation);
        return;
    }

    location[0] = player->tyres[TireIndex].pos[0];
    location[1] = player->tyres[TireIndex].baseHeight + ARCADEKART_DRIFT_TIRE_SPARK_HEIGHT_OFFSET;
    location[2] = player->tyres[TireIndex].pos[2];
    rotation[0] = 0;
    rotation[1] = player->rotation[1] + player->unk_0C0;
    rotation[2] = 0;
}

void ArcadeKartDriftTireSparkEmitter::InitializeParticle(ArcadeKartParticle& particle) {
    f32 upwardBias = 1.0f - (random_unit() * random_unit());

    particle.LocalPosition[0] += random_centered_unit() * ARCADEKART_DRIFT_TIRE_ZAP_JITTER_RADIUS;
    particle.LocalPosition[1] += random_centered_unit() * (ARCADEKART_DRIFT_TIRE_ZAP_JITTER_RADIUS * 0.35f);
    particle.LocalPosition[2] += random_centered_unit() * ARCADEKART_DRIFT_TIRE_ZAP_JITTER_RADIUS;
    particle.Velocity[0] = random_centered_unit() * ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_VELOCITY_X_RANGE;
    particle.Velocity[1] = ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_VELOCITY_Y_MIN +
                           ((ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_VELOCITY_Y_MAX -
                             ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_VELOCITY_Y_MIN) *
                            upwardBias);
    particle.Velocity[2] = -random_range(ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_BACKWARD_VELOCITY_MIN,
                                         ARCADEKART_DRIFT_TIRE_SPARK_LOCAL_BACKWARD_VELOCITY_MAX);
}

void ArcadeKartDriftTireSparkEmitter::UpdateParticle(ArcadeKartParticle& particle) {
    Vec3f emitterPosition;
    Vec3s emitterRotation;

    GetLocationAndRotation(emitterPosition, emitterRotation);
    offset_point_by_local_motion(emitterPosition, emitterRotation[1], particle.LocalPosition[0],
                                 particle.LocalPosition[1], particle.LocalPosition[2], particle.Position);
    particle.Rotation[0] = emitterRotation[0];
    particle.Rotation[1] = emitterRotation[1];
    particle.Rotation[2] = emitterRotation[2];
}

void ArcadeKartDriftTireSparkEmitter::DrawZapCore(s32 cameraId, const ArcadeKartParticle& particle) {
    Vec3f renderPosition;
    Vec3s rotation;
    s16 alpha = particle.Alpha;
    s16 tintRed;
    s16 tintGreen;
    s16 tintBlue;
    u8* leftTexture = particle.FlipHorizontal ? gTextureLoadedLightningBolt1 : gTextureLoadedLightningBolt0;
    u8* rightTexture = particle.FlipHorizontal ? gTextureLoadedLightningBolt0 : gTextureLoadedLightningBolt1;
    Vtx* leftVtx =
        particle.FlipHorizontal ? sArcadeKartDriftTireZapCoreLeftFlippedVtx : sArcadeKartDriftTireZapCoreLeftVtx;
    Vtx* rightVtx =
        particle.FlipHorizontal ? sArcadeKartDriftTireZapCoreRightFlippedVtx : sArcadeKartDriftTireZapCoreRightVtx;

    renderPosition[0] = particle.Position[0];
    renderPosition[1] = particle.Position[1];
    renderPosition[2] = particle.Position[2];
    get_stage_tint(DriftStage, &tintRed, &tintGreen, &tintBlue);

    rotation[0] = 0;
    rotation[1] = func_800418AC(renderPosition[0], renderPosition[2], camera1[cameraId].pos);
    rotation[2] = particle.BillboardRoll;

    FrameInterpolation_RecordOpenChild("arcadekart_drift_tire_zap_core",
                                        ((uintptr_t) PlayerId << 20) | ((uintptr_t) TireIndex << 16) |
                                            ((uintptr_t) cameraId << 12) | particle.SpawnId);
    func_800652D4(renderPosition, rotation, particle.ScaleMultiplier * ARCADEKART_DRIFT_TIRE_ZAP_CORE_SCALE);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D008DB8);
    gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIDECALA, G_CC_MODULATEIDECALA);
    gSPClearGeometryMode(gDisplayListHead++, G_ZBUFFER);
    gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPLoadTextureBlock(gDisplayListHead++, leftTexture, G_IM_FMT_IA, G_IM_SIZ_8b, 32, 64, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                        G_TX_NOLOD, G_TX_NOLOD);
    func_8004B35C(tintRed, tintGreen, tintBlue, alpha);
    gSPVertex(gDisplayListHead++, (uintptr_t) leftVtx, 4, 0);
    gSPDisplayList(gDisplayListHead++, (Gfx*) common_square_plain_render);
    gDPLoadTextureBlock(gDisplayListHead++, rightTexture, G_IM_FMT_IA, G_IM_SIZ_8b, 32, 64, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                        G_TX_NOLOD, G_TX_NOLOD);
    func_8004B35C(tintRed, tintGreen, tintBlue, alpha);
    gSPVertex(gDisplayListHead++, (uintptr_t) rightVtx, 4, 0);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D008DA0);
    gSPSetGeometryMode(gDisplayListHead++, G_ZBUFFER);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D007EB8);
    FrameInterpolation_RecordCloseChild();
    gMatrixEffectCount += 1;
}

void ArcadeKartDriftTireSparkEmitter::DrawExhaustSparkCopy(s32 cameraId, const ArcadeKartParticle& particle) {
    Vec3f sparkPosition;
    Vec3s rotation;
    s16 tintRed;
    s16 tintGreen;
    s16 tintBlue;
    f32 ageAmount = particle.AgeFrames / particle.LifetimeFrames;
    f32 side = (TireIndex == BACK_LEFT) ? -1.0f : 1.0f;
    f32 ageMotion = ageAmount * ageAmount;
    f32 wobble = ((f32) ((particle.SpawnId % 3) - 1)) * 0.25f * ageAmount;

    offset_point_by_local_motion(particle.Position, particle.Rotation[1],
                                 (side * ARCADEKART_DRIFT_TIRE_SPARK_SIDE_MOTION * ageMotion) + wobble +
                                     (particle.Velocity[0] * particle.AgeFrames),
                                 ARCADEKART_DRIFT_TIRE_SPARK_LIFT_MOTION * ageAmount +
                                     (particle.Velocity[1] * particle.AgeFrames),
                                 -ARCADEKART_DRIFT_TIRE_SPARK_BACK_MOTION * ageMotion +
                                     (particle.Velocity[2] * particle.AgeFrames),
                                 sparkPosition);
    get_stage_tint(DriftStage, &tintRed, &tintGreen, &tintBlue);

    rotation[0] = 0;
    rotation[1] = func_800418AC(sparkPosition[0], sparkPosition[2], camera1[cameraId].pos);
    rotation[2] = particle.BillboardRoll;

    FrameInterpolation_RecordOpenChild("arcadekart_drift_tire_exhaust_spark_copy",
                                        ((uintptr_t) PlayerId << 20) | ((uintptr_t) TireIndex << 16) |
                                            ((uintptr_t) cameraId << 12) | particle.SpawnId);
    func_800652D4(sparkPosition, rotation, particle.Scale * ARCADEKART_DRIFT_TIRE_SPARK_DRAW_SCALE);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D008DB8);
    gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
    gDPLoadTextureBlock(gDisplayListHead++, common_texture_particle_spark[particle.TextureIndex % 4], G_IM_FMT_I,
                        G_IM_SIZ_8b, 32, 32, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    func_8004B72C(tintRed, tintGreen, tintBlue, 0xBF, 0x20, 0x00, particle.Alpha);
    gSPClearGeometryMode(gDisplayListHead++, G_ZBUFFER);
    gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D008E48);
    gSPSetGeometryMode(gDisplayListHead++, G_ZBUFFER);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D007EB8);
    FrameInterpolation_RecordCloseChild();
    gMatrixEffectCount += 1;
}

void ArcadeKartDriftTireSparkEmitter::DrawParticle(s32 cameraId, const ArcadeKartParticle& particle) {
    DrawZapCore(cameraId, particle);
    DrawExhaustSparkCopy(cameraId, particle);
}
