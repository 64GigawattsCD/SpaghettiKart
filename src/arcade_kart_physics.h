#ifndef ARCADE_KART_PHYSICS_H
#define ARCADE_KART_PHYSICS_H

#include "arcade_kart_config.h"

void arcade_kart_physics_reset(const ArcadeKartConfig* config, ArcadeKartPhysicsState* state);
void arcade_kart_physics_step(const ArcadeKartConfig* config, ArcadeKartPhysicsState* state, const ArcadeKartInput* input,
                              const ArcadeKartEnvironment* environment, f32 dt, ArcadeKartPhysicsOutput* output);

#endif
