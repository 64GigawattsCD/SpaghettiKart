#pragma once

#include "ArcadeKartParticleEmitter.h"

class ArcadeKartBoostSparkEmitter : public ArcadeKartParticleEmitter {
public:
    explicit ArcadeKartBoostSparkEmitter(Player* player, s8 playerId, s8 side);

    void ConfigureForPlayer(Player* player, bool active);

protected:
    virtual void DrawParticle(s32 cameraId, const ArcadeKartParticle& particle) override;

private:
    s8 PlayerId = 0;
    s8 Side = 0;
};
