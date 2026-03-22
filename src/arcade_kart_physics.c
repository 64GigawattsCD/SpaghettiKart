#include "arcade_kart_physics.h"

#include "arcade_kart_differential.h"
#include "arcade_kart_engine.h"
#include "arcade_kart_transmission.h"
#include "arcade_kart_wheel.h"

#include <defines.h>
#include <math.h>
#include <macros.h>

static f32 clamp01(f32 value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static f32 clampf(f32 value, f32 minValue, f32 maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static f32 non_zero_sign(f32 value) {
    if (value < 0.0f) {
        return -1.0f;
    }
    return 1.0f;
}

static f32 get_driven_wheel_rpm(const ArcadeKartPhysicsState* state, f32 wheelRadius) {
    f32 totalAngular = 0.0f;
    s32 samples = 0;
    s32 i;

    if (state == NULL || wheelRadius <= 0.0f) {
        return 0.0f;
    }

    for (i = ARCADE_KART_WHEEL_REAR_LEFT; i <= ARCADE_KART_WHEEL_REAR_RIGHT; i++) {
        totalAngular += fabsf(state->wheels[i].angularSpeed);
        samples++;
    }
    if (samples == 0) {
        return 0.0f;
    }
    return (totalAngular / samples) * (60.0f / (2.0f * (f32) M_PI));
}

static void update_micro_boost(const ArcadeKartBoostConfig* boostConfig, ArcadeKartMicroBoostState* microState,
                               const ArcadeKartInput* input, f32 dt) {
    bool throttlePressed;

    if (boostConfig == NULL || microState == NULL || input == NULL) {
        return;
    }

    throttlePressed = (input->throttlePressed || (input->throttle > 0.8f && input->brake < 0.3f));
    microState->triggeredThisFrame = false;

    if (microState->tapWindowTimer > 0.0f) {
        microState->tapWindowTimer -= dt;
    }
    if (microState->cooldownTimer > 0.0f) {
        microState->cooldownTimer -= dt;
    }
    if (microState->boostTimer > 0.0f) {
        microState->boostTimer -= dt;
    }

    if (throttlePressed) {
        if (microState->tapWindowTimer > 0.0f && microState->cooldownTimer <= 0.0f) {
            microState->boostTimer = boostConfig->microBoostDuration;
            microState->cooldownTimer = boostConfig->microBoostCooldown;
            microState->tapWindowTimer = 0.0f;
            microState->triggeredThisFrame = true;
        } else {
            microState->tapWindowTimer = boostConfig->doubleTapWindow;
        }
    }
}

static void update_mini_turbo(const ArcadeKartConfig* config, ArcadeKartPhysicsState* state, const ArcadeKartInput* input,
                              f32 averageSlip, bool rearWheelsGrounded, f32 dt, bool* outTriggered) {
    f32 chargeDelta;
    s32 stage;

    if (config == NULL || state == NULL || input == NULL || outTriggered == NULL) {
        return;
    }

    *outTriggered = false;
    if (state->miniTurbo.boostTimer > 0.0f) {
        state->miniTurbo.boostTimer -= dt;
    }

    if (input->driftHeld && rearWheelsGrounded && fabsf(input->steer) > 0.1f && state->forwardSpeed >= config->drift.driftMinSpeed) {
        chargeDelta = config->drift.driftChargeRate;
        chargeDelta += fabsf(input->steer) * config->drift.driftChargeBySteer;
        chargeDelta += averageSlip * config->drift.driftChargeBySlip;
        state->miniTurbo.charge += chargeDelta * dt;

        stage = 0;
        if (state->miniTurbo.charge >= config->drift.miniTurboThresholds[0]) {
            stage = 1;
        }
        if (state->miniTurbo.charge >= config->drift.miniTurboThresholds[1]) {
            stage = 2;
        }
        if (state->miniTurbo.charge >= config->drift.miniTurboThresholds[2]) {
            stage = 3;
        }
        state->miniTurbo.stage = stage;
    } else if (state->miniTurbo.stage > 0) {
        stage = state->miniTurbo.stage - 1;
        state->miniTurbo.boostTimer = config->boost.miniTurboDurations[stage];
        state->miniTurbo.boostPower = config->boost.miniTurboPowers[stage];
        state->miniTurbo.charge = 0.0f;
        state->miniTurbo.stage = 0;
        *outTriggered = true;
    } else {
        state->miniTurbo.charge = MAX(0.0f, state->miniTurbo.charge - (dt * 0.25f));
        state->miniTurbo.stage = 0;
    }
}

void arcade_kart_physics_reset(const ArcadeKartConfig* config, ArcadeKartPhysicsState* state) {
    s32 i;

    if (config == NULL || state == NULL) {
        return;
    }

    arcade_kart_engine_reset(&config->engine, &state->engine);
    arcade_kart_transmission_reset(&config->transmission, &state->transmission, ARCADE_KART_TRANSMISSION_AUTOMATIC);
    arcade_kart_differential_reset(&state->differential);
    for (i = 0; i < ARCADE_KART_WHEEL_COUNT; i++) {
        arcade_kart_wheel_reset(&state->wheels[i]);
    }

    state->miniTurbo.charge = 0.0f;
    state->miniTurbo.stage = 0;
    state->miniTurbo.boostTimer = 0.0f;
    state->miniTurbo.boostPower = 0.0f;

    state->microBoost.tapWindowTimer = 0.0f;
    state->microBoost.cooldownTimer = 0.0f;
    state->microBoost.boostTimer = 0.0f;
    state->microBoost.triggeredThisFrame = false;

    state->forwardSpeed = 0.0f;
    state->lateralSpeed = 0.0f;
    state->yawVelocity = 0.0f;
    state->airborneTimer = 0.0f;
    state->lastThrottleInput = 0.0f;
    state->lastSteerInput = 0.0f;
    state->ffb = (ArcadeKartForceFeedbackSignals){ 0 };
}

void arcade_kart_physics_step(const ArcadeKartConfig* config, ArcadeKartPhysicsState* state, const ArcadeKartInput* input,
                              const ArcadeKartEnvironment* environment, f32 dt, ArcadeKartPhysicsOutput* output) {
    f32 currentGearRatio;
    f32 drivenWheelRpm;
    f32 loadFactor;
    f32 engineTorque;
    f32 engineBrakeTorque;
    f32 torqueScale;
    f32 totalDriveTorque;
    f32 wheelTorque[ARCADE_KART_WHEEL_COUNT] = { 0 };
    f32 wheelLongForce = 0.0f;
    f32 wheelLatForce = 0.0f;
    f32 wheelRollingForce = 0.0f;
    f32 averageSlip = 0.0f;
    f32 averageGrip = 0.0f;
    f32 averageRoughness = 0.0f;
    f32 averageBrakeScale = 0.0f;
    s32 contactCount = 0;
    s32 i;
    f32 boostPower;
    f32 boostTorque;
    f32 steerAuthority;
    f32 steerBlend;
    f32 yawGain;
    f32 yawDelta;
    f32 longitudinalAccel;
    f32 lateralAccel;
    f32 dragForce;
    f32 brakeForce;
    f32 offThrottleDecelForce;
    f32 signedSpeed;
    f32 speedSign;
    f32 clutchTransfer;
    f32 mass;
    bool miniTurboTriggered = false;
    ArcadeKartWheelForces wheelForces;
    bool rearWheelsGrounded;

    if (config == NULL || state == NULL || input == NULL || environment == NULL || output == NULL) {
        return;
    }
    if (dt <= 0.0f) {
        dt = TRACK_TIMER_ITER_f;
    }

    drivenWheelRpm = get_driven_wheel_rpm(state, config->wheel.wheelRadius);
    loadFactor = clamp01(1.0f - (fabsf(state->lateralSpeed) * 0.05f));

    arcade_kart_transmission_step(&config->transmission, &state->transmission, input, state->engine.rpm, dt);
    currentGearRatio = arcade_kart_transmission_get_gear_ratio(&config->transmission, &state->transmission);
    arcade_kart_engine_step(&config->engine, &state->engine, input->throttle, drivenWheelRpm, currentGearRatio, dt, loadFactor,
                            &engineTorque, &engineBrakeTorque);

    update_micro_boost(&config->boost, &state->microBoost, input, dt);
    rearWheelsGrounded = environment->wheelContact[ARCADE_KART_WHEEL_REAR_LEFT] ||
                         environment->wheelContact[ARCADE_KART_WHEEL_REAR_RIGHT];
    update_mini_turbo(config, state, input, clamp01(fabsf(state->lateralSpeed) / 6.0f), rearWheelsGrounded, dt,
                      &miniTurboTriggered);

    boostPower = 0.0f;
    if (state->miniTurbo.boostTimer > 0.0f) {
        boostPower += state->miniTurbo.boostPower;
    }
    if (state->microBoost.boostTimer > 0.0f) {
        boostPower += config->boost.microBoostPower;
    }
    boostPower = clamp01(boostPower);

    clutchTransfer = clamp01(1.0f - input->clutch);
    torqueScale = arcade_kart_transmission_get_torque_scale(&config->transmission, &state->transmission, input);
    boostTorque = config->engine.peakTorque * 0.35f * boostPower;
    totalDriveTorque = (engineTorque + boostTorque) * torqueScale;
    if (currentGearRatio < -0.001f) {
        totalDriveTorque = -totalDriveTorque;
    }
    arcade_kart_differential_split_torque(&config->differential, &state->differential, state->wheels, totalDriveTorque,
                                          wheelTorque);

    for (i = 0; i < ARCADE_KART_WHEEL_COUNT; i++) {
        const ArcadeKartSurfaceConfig* surfaceCfg;

        state->wheels[i].surfaceType = environment->wheelSurface[i];
        state->wheels[i].inContact = environment->wheelContact[i];
        surfaceCfg = &config->surfaces[state->wheels[i].surfaceType];
        if (state->wheels[i].inContact) {
            contactCount++;
        }

        arcade_kart_wheel_step(&config->wheel, surfaceCfg, &state->wheels[i], wheelTorque[i], input->brake,
                               (config->baseMass * 9.81f) * 0.25f, state->forwardSpeed, state->lateralSpeed,
                               input->driftHeld, (i == ARCADE_KART_WHEEL_FRONT_LEFT || i == ARCADE_KART_WHEEL_FRONT_RIGHT), dt,
                               &wheelForces);

        wheelLongForce += wheelForces.longitudinalForce;
        wheelLatForce += wheelForces.lateralForce;
        wheelRollingForce += wheelForces.rollingResistanceForce;
        averageSlip += wheelForces.slipIntensity;
        averageGrip += state->wheels[i].grip;
        averageRoughness += surfaceCfg->roughness;
        averageBrakeScale += surfaceCfg->brakeScale;
    }

    averageSlip /= (f32) ARCADE_KART_WHEEL_COUNT;
    averageGrip /= (f32) ARCADE_KART_WHEEL_COUNT;
    averageRoughness /= (f32) ARCADE_KART_WHEEL_COUNT;
    averageBrakeScale /= (f32) ARCADE_KART_WHEEL_COUNT;

    if (contactCount == 0) {
        state->airborneTimer += dt;
    } else {
        state->airborneTimer = 0.0f;
    }

    mass = MAX(1.0f, config->baseMass);
    signedSpeed = state->forwardSpeed;
    dragForce = config->aeroDrag * signedSpeed * fabsf(signedSpeed);
    brakeForce = input->brake * config->brakeForceScale * averageBrakeScale;
    offThrottleDecelForce = config->offThrottleDecel * (1.0f - input->throttle);

    longitudinalAccel = (wheelLongForce * config->accelForceScale) / mass;
    longitudinalAccel -= dragForce;
    longitudinalAccel -= wheelRollingForce * config->rollingDrag;
    if (fabsf(state->forwardSpeed) > 0.06f) {
        speedSign = non_zero_sign(state->forwardSpeed);
        longitudinalAccel -= brakeForce * speedSign;
        longitudinalAccel -= offThrottleDecelForce * speedSign;
        longitudinalAccel -= ((engineBrakeTorque * clutchTransfer) * 0.00045f) * speedSign;
    }

    if (contactCount == 0) {
        longitudinalAccel *= 0.30f;
    }

    lateralAccel = (wheelLatForce * 0.0017f) / mass;
    if (contactCount == 0) {
        lateralAccel *= config->air.lateralDamping;
    }

    state->forwardSpeed += longitudinalAccel;
    state->lateralSpeed += lateralAccel;

    if (state->forwardSpeed > config->baseTopSpeed) {
        state->forwardSpeed = config->baseTopSpeed;
    }
    if (state->forwardSpeed < -config->reverseTopSpeed) {
        state->forwardSpeed = -config->reverseTopSpeed;
    }

    state->lateralSpeed *= input->driftHeld ? 0.975f : 0.92f;
    if (contactCount == 0) {
        state->lateralSpeed *= 0.98f;
    }

    steerBlend = clamp01(fabsf(state->forwardSpeed) / MAX(0.1f, config->drift.steerAuthorityBlendSpeed));
    steerAuthority =
        (config->drift.steerAuthorityLowSpeed * (1.0f - steerBlend)) + (config->drift.steerAuthorityHighSpeed * steerBlend);
    yawGain = input->driftHeld ? config->drift.steerYawGainDrift : config->drift.steerYawGain;
    yawDelta = input->steer * steerAuthority * yawGain;
    yawDelta += clampf(state->lateralSpeed * 0.05f, -1.5f, 1.5f);

    if (contactCount == 0) {
        yawDelta *= config->air.yawAuthority;
    }
    state->yawVelocity = yawDelta;

    state->ffb.surfaceRoughness = clamp01(averageRoughness * (0.3f + (fabsf(state->forwardSpeed) * 0.09f)));
    state->ffb.slipIntensity = clamp01(averageSlip);
    state->ffb.loadCue = clamp01((fabsf(longitudinalAccel) * 0.10f) + (fabsf(lateralAccel) * 0.35f));
    state->ffb.steeringResistance = clamp01(fabsf(input->steer) * averageGrip * (fabsf(state->forwardSpeed) / 8.0f));
    state->ffb.airborneHint = clamp01((contactCount < 2) ? 1.0f : 0.0f);
    state->ffb.drivetrainVibe = clamp01((state->engine.rpm / config->engine.redlineRpm) * (0.5f + input->throttle * 0.5f));

    output->forwardSpeed = state->forwardSpeed;
    output->lateralSpeed = state->lateralSpeed;
    output->yawDelta = state->yawVelocity;
    output->longitudinalAccel = longitudinalAccel;
    output->lateralAccel = lateralAccel;
    output->engineRpm = state->engine.rpm;
    output->driveTorque = totalDriveTorque;
    output->boostPower = boostPower;
    output->miniTurboStage = state->miniTurbo.stage;
    output->miniTurboTriggered = miniTurboTriggered;
    output->microBoostTriggered = state->microBoost.triggeredThisFrame;
    output->airborne = (contactCount == 0);
    output->ffb = state->ffb;
    for (i = 0; i < ARCADE_KART_WHEEL_COUNT; i++) {
        output->wheelTorque[i] = wheelTorque[i];
        output->wheelBrakeTorque[i] = state->wheels[i].brakeTorque;
    }
}
