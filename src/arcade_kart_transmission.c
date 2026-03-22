#include "arcade_kart_transmission.h"

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

static s8 clamp_gear(const ArcadeKartTransmissionConfig* config, s8 gear) {
    if (gear > config->maxForwardGear) {
        return config->maxForwardGear;
    }
    if (gear < -1) {
        return -1;
    }
    return gear;
}

static void start_shift(const ArcadeKartTransmissionConfig* config, ArcadeKartTransmissionState* state, s8 nextGear,
                        f32 engineRpm) {
    bool perfectShift = false;
    f32 shiftTime = config->shiftLatencySeconds;

    if (state->shiftTimer > 0.0f || state->currentGear == nextGear) {
        return;
    }

    if ((state->mode == ARCADE_KART_TRANSMISSION_MANUAL) && (nextGear > state->currentGear) &&
        (fabsf(engineRpm - config->upshiftRpm) <= config->perfectShiftWindowRpm)) {
        perfectShift = true;
    }

    if (state->mode == ARCADE_KART_TRANSMISSION_AUTOMATIC) {
        shiftTime *= 0.78f;
    } else if (perfectShift) {
        shiftTime *= 0.56f;
        state->perfectShiftTimer = config->perfectShiftBoostDuration;
        state->missedShiftTimer = 0.0f;
    } else {
        shiftTime *= 1.10f;
        state->missedShiftTimer = 0.18f;
    }

    state->targetGear = nextGear;
    state->shiftTimer = MAX(0.01f, shiftTime);
}

void arcade_kart_transmission_reset(const ArcadeKartTransmissionConfig* config, ArcadeKartTransmissionState* state,
                                    ArcadeKartTransmissionMode mode) {
    if (config == NULL || state == NULL) {
        return;
    }

    state->mode = mode;
    state->currentGear = 1;
    state->targetGear = 1;
    state->shiftTimer = 0.0f;
    state->perfectShiftTimer = 0.0f;
    state->missedShiftTimer = 0.0f;
}

void arcade_kart_transmission_step(const ArcadeKartTransmissionConfig* config, ArcadeKartTransmissionState* state,
                                   const ArcadeKartInput* input, f32 engineRpm, f32 dt) {
    s8 nextGear = 0;

    if (config == NULL || state == NULL || input == NULL) {
        return;
    }

    state->mode = input->transmissionMode;

    if (state->perfectShiftTimer > 0.0f) {
        state->perfectShiftTimer -= dt;
    }
    if (state->missedShiftTimer > 0.0f) {
        state->missedShiftTimer -= dt;
    }

    if (state->shiftTimer > 0.0f) {
        state->shiftTimer -= dt;
        if (state->shiftTimer <= 0.0f) {
            state->currentGear = state->targetGear;
            state->shiftTimer = 0.0f;
        }
        return;
    }

    nextGear = state->currentGear;
    if (state->mode == ARCADE_KART_TRANSMISSION_AUTOMATIC) {
        if ((engineRpm > config->upshiftRpm) && (input->throttle > 0.25f)) {
            nextGear++;
        } else if ((engineRpm < config->downshiftRpm) || ((input->brake > 0.7f) && (state->currentGear > 1))) {
            nextGear--;
        }
    } else {
        if (input->requestedGear != 0) {
            nextGear = input->requestedGear;
        } else if (input->shiftUpPressed) {
            nextGear++;
        } else if (input->shiftDownPressed) {
            nextGear--;
        }
    }

    nextGear = clamp_gear(config, nextGear);
    if (nextGear == 0) {
        nextGear = 1;
    }
    start_shift(config, state, nextGear, engineRpm);
}

f32 arcade_kart_transmission_get_gear_ratio(const ArcadeKartTransmissionConfig* config,
                                            const ArcadeKartTransmissionState* state) {
    s8 gearIndex;

    if (config == NULL || state == NULL) {
        return 0.0f;
    }

    if (state->currentGear < 0) {
        return -(config->reverseRatio * config->finalDrive);
    }
    if (state->currentGear == 0) {
        return 0.0f;
    }

    gearIndex = state->currentGear - 1;
    if (gearIndex < 0 || gearIndex >= config->maxForwardGear) {
        return 0.0f;
    }
    return config->gearRatios[gearIndex] * config->finalDrive;
}

f32 arcade_kart_transmission_get_torque_scale(const ArcadeKartTransmissionConfig* config,
                                              const ArcadeKartTransmissionState* state,
                                              const ArcadeKartInput* input) {
    f32 clutch;
    f32 scale = 1.0f;

    if (config == NULL || state == NULL) {
        return scale;
    }

    if (state->shiftTimer > 0.0f) {
        scale *= 0.24f;
    }

    if (state->mode == ARCADE_KART_TRANSMISSION_AUTOMATIC) {
        scale *= config->autoTorqueScale;
    }
    if (state->perfectShiftTimer > 0.0f) {
        scale *= config->manualPerfectShiftTorqueScale;
    }
    if (state->missedShiftTimer > 0.0f) {
        scale *= 0.94f;
    }

    clutch = (input != NULL) ? clamp01(input->clutch) : 0.0f;
    scale *= (1.0f - clutch);
    return scale;
}
