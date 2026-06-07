#pragma once

#include <cstdint>
#include <vector>

#include "ParticleEmitter.h"

enum class ArcadeKartParticleSpace : uint8_t {
    World,
    Local,
};

typedef struct ArcadeKartParticleParams {
    ArcadeKartParticleSpace Space = ArcadeKartParticleSpace::World;
    bool Active = false;
    bool Looping = true;
    uint16_t MaxParticles = 64;
    f32 SpawnRatePerSecond = 0.0f;
    f32 LifetimeFrames = 30.0f;
    Vec3f LocalOffset = { 0.0f, 0.0f, 0.0f };
    Vec3f InitialVelocity = { 0.0f, 0.0f, 0.0f };
    Vec3f Acceleration = { 0.0f, 0.0f, 0.0f };
    f32 InitialScale = 1.0f;
    f32 FinalScale = 1.0f;
    s16 InitialAlpha = 0xFF;
    s16 FinalAlpha = 0;
} ArcadeKartParticleParams;

typedef struct ArcadeKartParticle {
    bool Active = false;
    f32 AgeFrames = 0.0f;
    f32 LifetimeFrames = 0.0f;
    Vec3f Position = { 0.0f, 0.0f, 0.0f };
    Vec3f LocalPosition = { 0.0f, 0.0f, 0.0f };
    Vec3f Velocity = { 0.0f, 0.0f, 0.0f };
    Vec3s Rotation = { 0, 0, 0 };
    f32 Scale = 1.0f;
    s16 Alpha = 0xFF;
} ArcadeKartParticle;

class ArcadeKartParticleEmitter : public ParticleEmitter {
public:
    explicit ArcadeKartParticleEmitter();
    explicit ArcadeKartParticleEmitter(const ArcadeKartParticleParams& params);

    void SetParameters(const ArcadeKartParticleParams& params);
    const ArcadeKartParticleParams& GetParameters() const;

    void SetActive(bool active);
    void SetMaxParticles(uint16_t maxParticles);
    void SetWorldTransform(f32 x, f32 y, f32 z, s16 pitch, s16 yaw, s16 roll);
    void SetLocalOffset(f32 x, f32 y, f32 z);
    void AttachToPlayer(Player* player);
    void Detach();
    void Clear();
    void SpawnBurst(uint16_t count);

    virtual void Tick() override;
    virtual void Draw(s32 cameraId) override;
    virtual bool IsMod() override;

protected:
    virtual void GetLocationAndRotation(Vec3f location, Vec3s rotation) const;
    virtual void InitializeParticle(ArcadeKartParticle& particle);
    virtual void UpdateParticle(ArcadeKartParticle& particle);
    virtual void DrawParticle(s32 cameraId, const ArcadeKartParticle& particle);

    const std::vector<ArcadeKartParticle>& GetParticles() const;

private:
    ArcadeKartParticleParams Params;
    std::vector<ArcadeKartParticle> Particles;
    Player* AttachedPlayer = nullptr;
    Vec3f WorldPosition = { 0.0f, 0.0f, 0.0f };
    Vec3s WorldRotation = { 0, 0, 0 };
    f32 SpawnAccumulator = 0.0f;
    size_t NextParticleIndex = 0;

    ArcadeKartParticle* AllocateParticle();
    void SpawnOne();
};
