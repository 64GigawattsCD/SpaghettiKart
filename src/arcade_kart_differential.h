#ifndef ARCADE_KART_DIFFERENTIAL_H
#define ARCADE_KART_DIFFERENTIAL_H

#include "arcade_kart_config.h"

void arcade_kart_differential_reset(ArcadeKartDifferentialState* state);
void arcade_kart_differential_split_torque(const ArcadeKartDifferentialConfig* config,
                                           ArcadeKartDifferentialState* state,
                                           const ArcadeKartWheelState wheels[ARCADE_KART_WHEEL_COUNT], f32 driveTorque,
                                           f32 outWheelTorque[ARCADE_KART_WHEEL_COUNT]);

#endif
