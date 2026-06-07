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
    f32 RandomScaleMin = 1.0f;
    f32 RandomScaleMax = 1.0f;
    s16 InitialRed = 0xFF;
    s16 InitialGreen = 0xFF;
    s16 InitialBlue = 0xFF;
    s16 FinalRed = 0xFF;
    s16 FinalGreen = 0xFF;
    s16 FinalBlue = 0xFF;
    f32 ColorPower = 1.0f;
    bool ColorOverLife = true;
    s16 InitialAlpha = 0xFF;
    s16 FinalAlpha = 0;
    f32 AlphaPower = 1.0f;
    uint8_t TextureIndex = 0;
    uint8_t TextureCount = 1;
    bool RandomTexture = false;
    bool RandomBillboardRoll = false;
    bool RandomHorizontalFlip = false;
    s16 InitialBillboardRoll = 0;
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
    f32 ScaleMultiplier = 1.0f;
    s16 Red = 0xFF;
    s16 Green = 0xFF;
    s16 Blue = 0xFF;
    s16 Alpha = 0xFF;
    uint8_t TextureIndex = 0;
    s16 BillboardRoll = 0;
    bool FlipHorizontal = false;
    uint16_t SpawnId = 0;
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
    Player* GetAttachedPlayer() const;

private:
    ArcadeKartParticleParams Params;
    std::vector<ArcadeKartParticle> Particles;
    Player* AttachedPlayer = nullptr;
    Vec3f WorldPosition = { 0.0f, 0.0f, 0.0f };
    Vec3s WorldRotation = { 0, 0, 0 };
    f32 SpawnAccumulator = 0.0f;
    size_t NextParticleIndex = 0;
    uint16_t NextSpawnId = 0;

    ArcadeKartParticle* AllocateParticle();
    void SpawnOne();
};
