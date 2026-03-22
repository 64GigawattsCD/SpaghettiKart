#include "arcade_kart_differential.h"

#include <math.h>
#include <macros.h>

static f32 clampf(f32 value, f32 minValue, f32 maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static void split_axle_torque(const ArcadeKartDifferentialConfig* config, ArcadeKartDifferentialType type,
                              f32 axleTorque, const ArcadeKartWheelState* leftWheel,
                              const ArcadeKartWheelState* rightWheel, f32* outLeft, f32* outRight, f32* outLock) {
    f32 baseLeft;
    f32 baseRight;
    f32 bias = 0.0f;
    f32 gripDelta;
    f32 slipDelta;
    f32 leftGrip = 1.0f;
    f32 rightGrip = 1.0f;

    if (leftWheel != NULL) {
        leftGrip = leftWheel->inContact ? MAX(0.05f, leftWheel->grip) : 0.05f;
    }
    if (rightWheel != NULL) {
        rightGrip = rightWheel->inContact ? MAX(0.05f, rightWheel->grip) : 0.05f;
    }

    gripDelta = leftGrip - rightGrip;
    slipDelta = 0.0f;
    if (leftWheel != NULL && rightWheel != NULL) {
        slipDelta = fabsf(leftWheel->longitudinalSlip) - fabsf(rightWheel->longitudinalSlip);
    }

    switch (type) {
        case ARCADE_KART_DIFF_OPEN:
            bias = clampf((gripDelta * 0.35f) - (slipDelta * 0.22f), -0.30f, 0.30f);
            break;
        case ARCADE_KART_DIFF_SPOOL:
            bias = 0.0f;
            break;
        case ARCADE_KART_DIFF_LSD:
            bias = clampf((gripDelta * config->lsdStrength) - (slipDelta * config->lockStrength), -0.45f, 0.45f);
            break;
    }

    baseLeft = axleTorque * (0.5f + (bias * 0.5f));
    baseRight = axleTorque * (0.5f - (bias * 0.5f));

    if (leftWheel != NULL && !leftWheel->inContact) {
        if (type == ARCADE_KART_DIFF_LSD || type == ARCADE_KART_DIFF_SPOOL) {
            baseRight += baseLeft * 0.70f;
        }
        baseLeft *= 0.20f;
    }
    if (rightWheel != NULL && !rightWheel->inContact) {
        if (type == ARCADE_KART_DIFF_LSD || type == ARCADE_KART_DIFF_SPOOL) {
            baseLeft += baseRight * 0.70f;
        }
        baseRight *= 0.20f;
    }

    if (outLeft != NULL) {
        *outLeft = baseLeft;
    }
    if (outRight != NULL) {
        *outRight = baseRight;
    }
    if (outLock != NULL) {
        *outLock = fabsf(bias);
    }
}

void arcade_kart_differential_reset(ArcadeKartDifferentialState* state) {
    if (state == NULL) {
        return;
    }
    state->lockAmount = 0.0f;
}

void arcade_kart_differential_split_torque(const ArcadeKartDifferentialConfig* config,
                                           ArcadeKartDifferentialState* state,
                                           const ArcadeKartWheelState wheels[ARCADE_KART_WHEEL_COUNT], f32 driveTorque,
                                           f32 outWheelTorque[ARCADE_KART_WHEEL_COUNT]) {
    f32 frontTorque;
    f32 rearTorque;
    f32 frontLock;
    f32 rearLock;

    if (config == NULL || outWheelTorque == NULL) {
        return;
    }

    outWheelTorque[ARCADE_KART_WHEEL_FRONT_LEFT] = 0.0f;
    outWheelTorque[ARCADE_KART_WHEEL_FRONT_RIGHT] = 0.0f;
    outWheelTorque[ARCADE_KART_WHEEL_REAR_LEFT] = 0.0f;
    outWheelTorque[ARCADE_KART_WHEEL_REAR_RIGHT] = 0.0f;

    frontTorque = driveTorque * clampf(config->frontTorqueSplit, 0.0f, 1.0f);
    rearTorque = driveTorque - frontTorque;

    split_axle_torque(config, config->type, frontTorque, &wheels[ARCADE_KART_WHEEL_FRONT_LEFT],
                      &wheels[ARCADE_KART_WHEEL_FRONT_RIGHT], &outWheelTorque[ARCADE_KART_WHEEL_FRONT_LEFT],
                      &outWheelTorque[ARCADE_KART_WHEEL_FRONT_RIGHT], &frontLock);

    split_axle_torque(config, config->type, rearTorque, &wheels[ARCADE_KART_WHEEL_REAR_LEFT],
                      &wheels[ARCADE_KART_WHEEL_REAR_RIGHT], &outWheelTorque[ARCADE_KART_WHEEL_REAR_LEFT],
                      &outWheelTorque[ARCADE_KART_WHEEL_REAR_RIGHT], &rearLock);

    if (state != NULL) {
        state->lockAmount = 0.5f * (frontLock + rearLock);
    }
}
