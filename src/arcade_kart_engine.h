#ifndef ARCADE_KART_ENGINE_H
#define ARCADE_KART_ENGINE_H

#include "arcade_kart_config.h"

void arcade_kart_engine_reset(const ArcadeKartEngineConfig* config, ArcadeKartEngineState* state);
f32 arcade_kart_engine_sample_torque_curve(const ArcadeKartEngineConfig* config, f32 rpm);
void arcade_kart_engine_step(const ArcadeKartEngineConfig* config, ArcadeKartEngineState* state, f32 throttleInput,
                             f32 wheelRpm, f32 gearRatio, f32 dt, f32 loadFactor, f32* outDriveTorque,
                             f32* outEngineBrakeTorque);

#endif
