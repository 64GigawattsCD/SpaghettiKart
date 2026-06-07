#pragma once

#include "ArcadeKartParticleEmitter.h"

class ArcadeKartDriftTireSparkEmitter : public ArcadeKartParticleEmitter {
public:
    explicit ArcadeKartDriftTireSparkEmitter(Player* player, s8 playerId, s8 tireIndex);

    void ConfigureForPlayer(Player* player, bool active);

protected:
    virtual void GetLocationAndRotation(Vec3f location, Vec3s rotation) const override;
    virtual void InitializeParticle(ArcadeKartParticle& particle) override;
    virtual void DrawParticle(s32 cameraId, const ArcadeKartParticle& particle) override;
    void DrawZapCore(s32 cameraId, const ArcadeKartParticle& particle);
    void DrawExhaustSparkCopy(s32 cameraId, const ArcadeKartParticle& particle);

private:
    s8 PlayerId = 0;
    s8 TireIndex = 0;
};
