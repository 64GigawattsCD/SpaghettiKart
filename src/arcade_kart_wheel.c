#include "arcade_kart_wheel.h"

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

static f32 signed_non_zero(f32 value) {
    if (value >= 0.0f) {
        return 1.0f;
    }
    return -1.0f;
}

void arcade_kart_wheel_reset(ArcadeKartWheelState* state) {
    if (state == NULL) {
        return;
    }

    state->inContact = true;
    state->surfaceType = ARCADE_KART_SURFACE_ROAD;
    state->angularSpeed = 0.0f;
    state->normalLoad = 0.0f;
    state->longitudinalSlip = 0.0f;
    state->lateralSlip = 0.0f;
    state->grip = 1.0f;
    state->driveTorque = 0.0f;
    state->brakeTorque = 0.0f;
}

void arcade_kart_wheel_step(const ArcadeKartWheelConfig* wheelConfig, const ArcadeKartSurfaceConfig* surfaceConfig,
                            ArcadeKartWheelState* state, f32 driveTorque, f32 brakeInput, f32 normalLoad,
                            f32 vehicleForwardSpeed, f32 vehicleLateralSpeed, bool drifting, bool steeringAxle, f32 dt,
                            ArcadeKartWheelForces* outForces) {
    f32 gripScale;
    f32 maxLongForce;
    f32 maxLatForce;
    f32 driveForce;
    f32 brakeForce;
    f32 rollingForce;
    f32 longitudinalForce;
    f32 lateralForce;
    f32 wheelLinearSpeed;
    f32 longSlip;
    f32 latSlip;
    f32 slipIntensity;

    if (wheelConfig == NULL || surfaceConfig == NULL || state == NULL || outForces == NULL) {
        return;
    }

    gripScale = wheelConfig->baseLongGrip * surfaceConfig->gripScale;
    if (drifting && !steeringAxle) {
        gripScale *= wheelConfig->driftRearLateralGripScale;
    }

    state->inContact = state->inContact && (state->surfaceType != ARCADE_KART_SURFACE_AIRBORNE);
    state->normalLoad = state->inContact ? normalLoad : 0.0f;
    state->driveTorque = driveTorque;
    state->brakeTorque = clampf(brakeInput, 0.0f, 1.0f) * wheelConfig->maxBrakeTorque * surfaceConfig->brakeScale;
    state->grip = gripScale;

    maxLongForce = state->normalLoad * gripScale;
    maxLatForce = state->normalLoad * wheelConfig->baseLatGrip * surfaceConfig->gripScale;
    if (drifting && !steeringAxle) {
        maxLatForce *= wheelConfig->driftRearLateralGripScale;
    }

    driveForce = (wheelConfig->wheelRadius > 0.0f) ? (state->driveTorque / wheelConfig->wheelRadius) : 0.0f;
    brakeForce = (wheelConfig->wheelRadius > 0.0f) ? (state->brakeTorque / wheelConfig->wheelRadius) : 0.0f;
    rollingForce = vehicleForwardSpeed * wheelConfig->rollingResistance * surfaceConfig->rollingResistanceScale;

    if (!state->inContact) {
        outForces->longitudinalForce = 0.0f;
        outForces->lateralForce = 0.0f;
        outForces->rollingResistanceForce = 0.0f;
        outForces->slipIntensity = 0.0f;
        state->longitudinalSlip = 0.0f;
        state->lateralSlip = 0.0f;
        return;
    }

    longitudinalForce = driveForce - (signed_non_zero(vehicleForwardSpeed + (driveForce * 0.001f)) * brakeForce) - rollingForce;
    longitudinalForce = clampf(longitudinalForce, -maxLongForce, maxLongForce);

    lateralForce = -(vehicleLateralSpeed * wheelConfig->lateralDamping * maxLatForce);
    lateralForce = clampf(lateralForce, -maxLatForce, maxLatForce);

    wheelLinearSpeed = state->angularSpeed * wheelConfig->wheelRadius;
    longSlip = (wheelLinearSpeed - vehicleForwardSpeed) / (fabsf(vehicleForwardSpeed) + 1.5f);
    latSlip = vehicleLateralSpeed / (fabsf(vehicleForwardSpeed) + 4.0f);

    state->longitudinalSlip = longSlip;
    state->lateralSlip = latSlip;

    state->angularSpeed += ((state->driveTorque - (state->brakeTorque * signed_non_zero(state->angularSpeed + 0.001f))) /
                            MAX(0.15f, wheelConfig->wheelInertia)) *
                           dt;
    state->angularSpeed += (vehicleForwardSpeed / MAX(0.05f, wheelConfig->wheelRadius) - state->angularSpeed) *
                           clampf(dt * (5.0f + (state->grip * 2.0f)), 0.0f, 1.0f);

    slipIntensity = MAX(fabsf(longSlip), fabsf(latSlip));
    slipIntensity = clampf(slipIntensity * wheelConfig->slipToFfbScale, 0.0f, 1.0f);

    outForces->longitudinalForce = longitudinalForce;
    outForces->lateralForce = lateralForce;
    outForces->rollingResistanceForce = rollingForce;
    outForces->slipIntensity = slipIntensity;
}
