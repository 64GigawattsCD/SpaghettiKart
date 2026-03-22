#ifndef ARCADE_KART_WHEEL_H
#define ARCADE_KART_WHEEL_H

#include "arcade_kart_config.h"

typedef struct {
    f32 longitudinalForce;
    f32 lateralForce;
    f32 rollingResistanceForce;
    f32 slipIntensity;
} ArcadeKartWheelForces;

void arcade_kart_wheel_reset(ArcadeKartWheelState* state);
void arcade_kart_wheel_step(const ArcadeKartWheelConfig* wheelConfig, const ArcadeKartSurfaceConfig* surfaceConfig,
                            ArcadeKartWheelState* state, f32 driveTorque, f32 brakeInput, f32 normalLoad,
                            f32 vehicleForwardSpeed, f32 vehicleLateralSpeed, bool drifting, bool steeringAxle, f32 dt,
                            ArcadeKartWheelForces* outForces);

#endif
