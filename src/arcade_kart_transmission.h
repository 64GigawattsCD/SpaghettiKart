#ifndef ARCADE_KART_TRANSMISSION_H
#define ARCADE_KART_TRANSMISSION_H

#include "arcade_kart_config.h"

void arcade_kart_transmission_reset(const ArcadeKartTransmissionConfig* config, ArcadeKartTransmissionState* state,
                                    ArcadeKartTransmissionMode mode);
void arcade_kart_transmission_step(const ArcadeKartTransmissionConfig* config, ArcadeKartTransmissionState* state,
                                   const ArcadeKartInput* input, f32 engineRpm, f32 dt);
f32 arcade_kart_transmission_get_gear_ratio(const ArcadeKartTransmissionConfig* config,
                                            const ArcadeKartTransmissionState* state);
f32 arcade_kart_transmission_get_torque_scale(const ArcadeKartTransmissionConfig* config,
                                              const ArcadeKartTransmissionState* state,
                                              const ArcadeKartInput* input);

#endif
