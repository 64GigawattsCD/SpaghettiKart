#include "ArcadeKartBoostSparkEmitter.h"

#include "port/interpolation/FrameInterpolation.h"

extern "C" {
#include "assets/models/common_data.h"
#include "assets/textures/common_data.h"
#include "camera.h"
#include "code_80057C60.h"
#include "defines.h"
#include "main.h"
#include "math_util_2.h"
#include "render_objects.h"
#include "render_player.h"
}

#define ARCADEKART_BOOST_SPARK_MAX_PARTICLES 24
#define ARCADEKART_BOOST_SPARK_SPAWN_RATE 10.0f
#define ARCADEKART_BOOST_SPARK_LIFETIME_FRAMES 18.0f
#define ARCADEKART_BOOST_SPARK_SIDE_OFFSET 1.458f
#define ARCADEKART_BOOST_SPARK_LOCAL_Y 5.2f
#define ARCADEKART_BOOST_SPARK_LOCAL_Z -6.7f
#define ARCADEKART_BOOST_SPARK_LOCAL_VELOCITY_Y 0.24f
#define ARCADEKART_BOOST_SPARK_LOCAL_VELOCITY_Z -0.55f
#define ARCADEKART_BOOST_SPARK_INITIAL_SCALE 1.19f
#define ARCADEKART_BOOST_SPARK_FINAL_SCALE 0.38f
#define ARCADEKART_BOOST_SPARK_ALPHA_POWER 2.8f
#define ARCADEKART_BOOST_SPARK_COLOR_POWER 1.0f

static ArcadeKartParticleParams make_boost_spark_params(s8 side) {
    ArcadeKartParticleParams params;

    params.Space = ArcadeKartParticleSpace::Local;
    params.Active = false;
    params.Looping = true;
    params.MaxParticles = ARCADEKART_BOOST_SPARK_MAX_PARTICLES;
    params.SpawnRatePerSecond = ARCADEKART_BOOST_SPARK_SPAWN_RATE;
    params.LifetimeFrames = ARCADEKART_BOOST_SPARK_LIFETIME_FRAMES;
    params.LocalOffset[0] = ARCADEKART_BOOST_SPARK_SIDE_OFFSET * side;
    params.LocalOffset[1] = ARCADEKART_BOOST_SPARK_LOCAL_Y;
    params.LocalOffset[2] = ARCADEKART_BOOST_SPARK_LOCAL_Z;
    params.InitialVelocity[0] = 0.0f;
    params.InitialVelocity[1] = ARCADEKART_BOOST_SPARK_LOCAL_VELOCITY_Y;
    params.InitialVelocity[2] = ARCADEKART_BOOST_SPARK_LOCAL_VELOCITY_Z;
    params.Acceleration[0] = 0.0f;
    params.Acceleration[1] = 0.0f;
    params.Acceleration[2] = 0.0f;
    params.InitialScale = ARCADEKART_BOOST_SPARK_INITIAL_SCALE;
    params.FinalScale = ARCADEKART_BOOST_SPARK_FINAL_SCALE;
    params.InitialRed = 0xFF;
    params.InitialGreen = 0xE4;
    params.InitialBlue = 0x38;
    params.FinalRed = 0xFF;
    params.FinalGreen = 0x54;
    params.FinalBlue = 0x10;
    params.ColorPower = ARCADEKART_BOOST_SPARK_COLOR_POWER;
    params.InitialAlpha = 0xFF;
    params.FinalAlpha = 0;
    params.AlphaPower = ARCADEKART_BOOST_SPARK_ALPHA_POWER;
    params.TextureIndex = 0;
    params.TextureCount = 4;
    params.RandomTexture = true;
    params.RandomBillboardRoll = true;
    return params;
}

ArcadeKartBoostSparkEmitter::ArcadeKartBoostSparkEmitter(Player* player, s8 playerId, s8 side)
    : ArcadeKartParticleEmitter(make_boost_spark_params(side)), PlayerId(playerId), Side(side) {
    AttachToPlayer(player);
}

void ArcadeKartBoostSparkEmitter::ConfigureForPlayer(Player* player, bool active) {
    AttachToPlayer(player);
    SetLocalOffset(ARCADEKART_BOOST_SPARK_SIDE_OFFSET * Side,
                   ARCADEKART_BOOST_SPARK_LOCAL_Y - player->boundingBoxSize,
                   ARCADEKART_BOOST_SPARK_LOCAL_Z);
    SetActive(active);
}

void ArcadeKartBoostSparkEmitter::DrawParticle(s32 cameraId, const ArcadeKartParticle& particle) {
    Vec3s rotation;

    rotation[0] = 0;
    rotation[1] = func_800418AC(particle.Position[0], particle.Position[2], camera1[cameraId].pos);
    rotation[2] = particle.BillboardRoll;

    FrameInterpolation_RecordOpenChild("arcadekart_boost_exhaust_spark",
                                        ((uintptr_t) PlayerId << 20) | ((uintptr_t) (Side > 0) << 16) |
                                            ((uintptr_t) cameraId << 12) | particle.SpawnId);
    func_800652D4((f32*) particle.Position, rotation, particle.Scale);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D008DB8);
    gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
    gDPLoadTextureBlock(gDisplayListHead++, common_texture_particle_spark[particle.TextureIndex % 4], G_IM_FMT_I,
                        G_IM_SIZ_8b, 32, 32, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    func_8004B72C(particle.Red, particle.Green, particle.Blue, 0xBF, 0x20, 0x00, particle.Alpha);
    gDPSetRenderMode(gDisplayListHead++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D008E48);
    gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D007EB8);
    FrameInterpolation_RecordCloseChild();
    gMatrixEffectCount += 1;
}
