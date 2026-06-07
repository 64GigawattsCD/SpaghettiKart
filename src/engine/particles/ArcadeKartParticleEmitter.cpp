#include "ArcadeKartParticleEmitter.h"

#include <algorithm>

extern "C" {
#include "math_util.h"
}

static void copy_vec3f(Vec3f dest, const Vec3f src) {
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
}

static void copy_vec3s(Vec3s dest, const Vec3s src) {
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
}

static f32 lerp_f32(f32 start, f32 end, f32 amount) {
    return start + ((end - start) * amount);
}

static s16 lerp_s16(s16 start, s16 end, f32 amount) {
    return (s16) (lerp_f32((f32) start, (f32) end, amount));
}

static void transform_local_vector_yaw(const Vec3s rotation, const Vec3f local, Vec3f world) {
    f32 yawSin = sins(rotation[1]);
    f32 yawCos = coss(rotation[1]);

    world[0] = (local[0] * yawCos) + (local[2] * yawSin);
    world[1] = local[1];
    world[2] = (local[2] * yawCos) - (local[0] * yawSin);
}

static void transform_local_point_yaw(const Vec3f origin, const Vec3s rotation, const Vec3f local, Vec3f world) {
    Vec3f rotated;

    transform_local_vector_yaw(rotation, local, rotated);
    world[0] = origin[0] + rotated[0];
    world[1] = origin[1] + rotated[1];
    world[2] = origin[2] + rotated[2];
}

ArcadeKartParticleEmitter::ArcadeKartParticleEmitter() {
    SetMaxParticles(Params.MaxParticles);
}

ArcadeKartParticleEmitter::ArcadeKartParticleEmitter(const ArcadeKartParticleParams& params) {
    SetParameters(params);
}

void ArcadeKartParticleEmitter::SetParameters(const ArcadeKartParticleParams& params) {
    Params = params;
    SetMaxParticles(Params.MaxParticles);
}

const ArcadeKartParticleParams& ArcadeKartParticleEmitter::GetParameters() const {
    return Params;
}

void ArcadeKartParticleEmitter::SetActive(bool active) {
    Params.Active = active;
    if (!active) {
        SpawnAccumulator = 0.0f;
    }
}

void ArcadeKartParticleEmitter::SetMaxParticles(uint16_t maxParticles) {
    if (maxParticles == 0) {
        maxParticles = 1;
    }
    Params.MaxParticles = maxParticles;
    Particles.resize(maxParticles);
    if (NextParticleIndex >= Particles.size()) {
        NextParticleIndex = 0;
    }
}

void ArcadeKartParticleEmitter::SetWorldTransform(f32 x, f32 y, f32 z, s16 pitch, s16 yaw, s16 roll) {
    WorldPosition[0] = x;
    WorldPosition[1] = y;
    WorldPosition[2] = z;
    WorldRotation[0] = pitch;
    WorldRotation[1] = yaw;
    WorldRotation[2] = roll;
}

void ArcadeKartParticleEmitter::SetLocalOffset(f32 x, f32 y, f32 z) {
    Params.LocalOffset[0] = x;
    Params.LocalOffset[1] = y;
    Params.LocalOffset[2] = z;
}

void ArcadeKartParticleEmitter::AttachToPlayer(Player* player) {
    AttachedPlayer = player;
}

void ArcadeKartParticleEmitter::Detach() {
    AttachedPlayer = nullptr;
}

void ArcadeKartParticleEmitter::Clear() {
    for (ArcadeKartParticle& particle : Particles) {
        particle.Active = false;
    }
    SpawnAccumulator = 0.0f;
    NextParticleIndex = 0;
}

void ArcadeKartParticleEmitter::SpawnBurst(uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        SpawnOne();
    }
}

void ArcadeKartParticleEmitter::GetLocationAndRotation(Vec3f location, Vec3s rotation) const {
    if (AttachedPlayer != nullptr) {
        copy_vec3f(location, AttachedPlayer->pos);
        copy_vec3s(rotation, AttachedPlayer->rotation);
        return;
    }

    copy_vec3f(location, WorldPosition);
    copy_vec3s(rotation, WorldRotation);
}

ArcadeKartParticle* ArcadeKartParticleEmitter::AllocateParticle() {
    for (ArcadeKartParticle& particle : Particles) {
        if (!particle.Active) {
            return &particle;
        }
    }

    ArcadeKartParticle* particle = &Particles[NextParticleIndex];
    NextParticleIndex = (NextParticleIndex + 1) % Particles.size();
    return particle;
}

void ArcadeKartParticleEmitter::SpawnOne() {
    Vec3f emitterPosition;
    Vec3s emitterRotation;
    Vec3f worldVelocity;
    ArcadeKartParticle* particle = AllocateParticle();

    GetLocationAndRotation(emitterPosition, emitterRotation);
    particle->Active = true;
    particle->AgeFrames = 0.0f;
    particle->LifetimeFrames = std::max(1.0f, Params.LifetimeFrames);
    copy_vec3s(particle->Rotation, emitterRotation);
    copy_vec3f(particle->LocalPosition, Params.LocalOffset);
    transform_local_vector_yaw(emitterRotation, Params.InitialVelocity, worldVelocity);
    copy_vec3f(particle->Velocity, worldVelocity);

    if (Params.Space == ArcadeKartParticleSpace::Local) {
        transform_local_point_yaw(emitterPosition, emitterRotation, particle->LocalPosition, particle->Position);
    } else {
        transform_local_point_yaw(emitterPosition, emitterRotation, Params.LocalOffset, particle->Position);
    }

    particle->Scale = Params.InitialScale;
    particle->Alpha = Params.InitialAlpha;
    InitializeParticle(*particle);
}

void ArcadeKartParticleEmitter::InitializeParticle(ArcadeKartParticle& particle) {
    (void) particle;
}

void ArcadeKartParticleEmitter::UpdateParticle(ArcadeKartParticle& particle) {
    Vec3f emitterPosition;
    Vec3s emitterRotation;

    particle.Velocity[0] += Params.Acceleration[0];
    particle.Velocity[1] += Params.Acceleration[1];
    particle.Velocity[2] += Params.Acceleration[2];

    if (Params.Space == ArcadeKartParticleSpace::Local) {
        particle.LocalPosition[0] += particle.Velocity[0];
        particle.LocalPosition[1] += particle.Velocity[1];
        particle.LocalPosition[2] += particle.Velocity[2];
        GetLocationAndRotation(emitterPosition, emitterRotation);
        transform_local_point_yaw(emitterPosition, emitterRotation, particle.LocalPosition, particle.Position);
        copy_vec3s(particle.Rotation, emitterRotation);
    } else {
        particle.Position[0] += particle.Velocity[0];
        particle.Position[1] += particle.Velocity[1];
        particle.Position[2] += particle.Velocity[2];
    }
}

void ArcadeKartParticleEmitter::Tick() {
    if (Params.Active && (Params.SpawnRatePerSecond > 0.0f)) {
        SpawnAccumulator += Params.SpawnRatePerSecond / 60.0f;
        while (SpawnAccumulator >= 1.0f) {
            SpawnOne();
            SpawnAccumulator -= 1.0f;
            if (!Params.Looping) {
                Params.Active = false;
                SpawnAccumulator = 0.0f;
                break;
            }
        }
    }

    for (ArcadeKartParticle& particle : Particles) {
        f32 normalizedAge;

        if (!particle.Active) {
            continue;
        }

        UpdateParticle(particle);
        particle.AgeFrames += 1.0f;
        normalizedAge = particle.AgeFrames / particle.LifetimeFrames;
        if (normalizedAge >= 1.0f) {
            particle.Active = false;
            continue;
        }
        particle.Scale = lerp_f32(Params.InitialScale, Params.FinalScale, normalizedAge);
        particle.Alpha = lerp_s16(Params.InitialAlpha, Params.FinalAlpha, normalizedAge);
    }
}

void ArcadeKartParticleEmitter::Draw(s32 cameraId) {
    for (const ArcadeKartParticle& particle : Particles) {
        if (particle.Active) {
            DrawParticle(cameraId, particle);
        }
    }
}

void ArcadeKartParticleEmitter::DrawParticle(s32 cameraId, const ArcadeKartParticle& particle) {
    (void) cameraId;
    (void) particle;
}

const std::vector<ArcadeKartParticle>& ArcadeKartParticleEmitter::GetParticles() const {
    return Particles;
}

bool ArcadeKartParticleEmitter::IsMod() {
    return true;
}
