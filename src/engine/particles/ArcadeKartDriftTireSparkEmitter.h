#pragma once

#include "ArcadeKartParticleEmitter.h"

typedef enum ArcadeKartDriftTireSparkStage {
    ARCADEKART_DRIFT_TIRE_SPARK_STAGE_NONE,
    ARCADEKART_DRIFT_TIRE_SPARK_STAGE_SLIDE,
    ARCADEKART_DRIFT_TIRE_SPARK_STAGE_DRIFT,
    ARCADEKART_DRIFT_TIRE_SPARK_STAGE_TURBO,
} ArcadeKartDriftTireSparkStage;

class ArcadeKartDriftTireSparkEmitter : public ArcadeKartParticleEmitter {
public:
    explicit ArcadeKartDriftTireSparkEmitter(Player* player, s8 playerId, s8 tireIndex);

    void ConfigureForPlayer(Player* player, bool active, s8 driftStage);
    virtual bool DrawsInWorldParticlePass() override;

protected:
    virtual void GetLocationAndRotation(Vec3f location, Vec3s rotation) const override;
    virtual void InitializeParticle(ArcadeKartParticle& particle) override;
    virtual void UpdateParticle(ArcadeKartParticle& particle) override;
    virtual void DrawParticle(s32 cameraId, const ArcadeKartParticle& particle) override;
    void DrawZapCore(s32 cameraId, const ArcadeKartParticle& particle);
    void DrawExhaustSparkCopy(s32 cameraId, const ArcadeKartParticle& particle);

private:
    s8 PlayerId = 0;
    s8 TireIndex = 0;
    s8 DriftStage = ARCADEKART_DRIFT_TIRE_SPARK_STAGE_NONE;
};
