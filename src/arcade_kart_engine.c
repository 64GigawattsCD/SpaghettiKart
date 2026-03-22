#include "arcade_kart_engine.h"

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

void arcade_kart_engine_reset(const ArcadeKartEngineConfig* config, ArcadeKartEngineState* state) {
    if (config == NULL || state == NULL) {
        return;
    }

    state->rpm = config->idleRpm;
    state->throttleFiltered = 0.0f;
    state->lastDriveTorque = 0.0f;
    state->lastEngineBrakeTorque = 0.0f;
}

f32 arcade_kart_engine_sample_torque_curve(const ArcadeKartEngineConfig* config, f32 rpm) {
    f32 normRpm;
    f32 lowEnd;
    f32 highEndDrop;

    if (config == NULL || config->redlineRpm <= 0.0f) {
        return 0.0f;
    }

    normRpm = clamp01((rpm - config->idleRpm) / (config->redlineRpm - config->idleRpm));
    lowEnd = 1.0f + ((1.0f - normRpm) * (config->lowEndTorqueBoost - 1.0f));
    highEndDrop = 1.0f - (config->highRpmTorqueDropoff * (normRpm * normRpm));

    return MAX(0.0f, lowEnd * highEndDrop);
}

void arcade_kart_engine_step(const ArcadeKartEngineConfig* config, ArcadeKartEngineState* state, f32 throttleInput,
                             f32 wheelRpm, f32 gearRatio, f32 dt, f32 loadFactor, f32* outDriveTorque,
                             f32* outEngineBrakeTorque) {
    f32 throttleBlend;
    f32 throttleTarget;
    f32 coupling;
    f32 targetRpm;
    f32 freeRevRpm;
    f32 torqueCurve;
    f32 engineTorque;
    f32 engineBrake;

    if (config == NULL || state == NULL) {
        return;
    }

    throttleTarget = clamp01(throttleInput);
    throttleBlend = clamp01(dt * config->throttleResponse);
    state->throttleFiltered += (throttleTarget - state->throttleFiltered) * throttleBlend;

    freeRevRpm = config->idleRpm + (state->throttleFiltered * (config->redlineRpm - config->idleRpm));
    targetRpm = freeRevRpm;
    if (fabsf(gearRatio) > 0.01f) {
        coupling = clamp01(0.35f + (0.65f * loadFactor));
        targetRpm = MAX(config->idleRpm, fabsf(wheelRpm * gearRatio));
        targetRpm = (targetRpm * coupling) + (freeRevRpm * (1.0f - coupling));
    }

    state->rpm += (targetRpm - state->rpm) * clamp01(dt * config->rpmResponse);
    state->rpm = MIN(config->maxRpm, MAX(config->idleRpm, state->rpm));

    torqueCurve = arcade_kart_engine_sample_torque_curve(config, state->rpm);
    engineTorque = config->peakTorque * torqueCurve * state->throttleFiltered;
    engineBrake = config->engineBrakeTorque * (1.0f - state->throttleFiltered) * (state->rpm / config->redlineRpm);

    state->lastDriveTorque = engineTorque;
    state->lastEngineBrakeTorque = engineBrake;

    if (outDriveTorque != NULL) {
        *outDriveTorque = engineTorque;
    }
    if (outEngineBrakeTorque != NULL) {
        *outEngineBrakeTorque = engineBrake;
    }
}
