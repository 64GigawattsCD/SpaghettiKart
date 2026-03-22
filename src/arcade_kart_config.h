#ifndef ARCADE_KART_CONFIG_H
#define ARCADE_KART_CONFIG_H

#include "arcade_kart_types.h"

typedef struct {
    f32 idleRpm;
    f32 redlineRpm;
    f32 maxRpm;
    f32 throttleResponse;
    f32 rpmResponse;
    f32 peakTorque;
    f32 lowEndTorqueBoost;
    f32 highRpmTorqueDropoff;
    f32 engineBrakeTorque;
} ArcadeKartEngineConfig;

typedef struct {
    f32 gearRatios[ARCADE_KART_MAX_GEARS];
    s8 maxForwardGear;
    f32 reverseRatio;
    f32 finalDrive;
    f32 upshiftRpm;
    f32 downshiftRpm;
    f32 shiftLatencySeconds;
    f32 autoTorqueScale;
    f32 perfectShiftWindowRpm;
    f32 perfectShiftBoostDuration;
    f32 manualPerfectShiftTorqueScale;
} ArcadeKartTransmissionConfig;

typedef struct {
    ArcadeKartDifferentialType type;
    f32 frontTorqueSplit;
    f32 lockStrength;
    f32 lsdStrength;
} ArcadeKartDifferentialConfig;

typedef struct {
    f32 wheelRadius;
    f32 wheelInertia;
    f32 baseLongGrip;
    f32 baseLatGrip;
    f32 rollingResistance;
    f32 lateralDamping;
    f32 maxBrakeTorque;
    f32 driftRearLateralGripScale;
    f32 slipToFfbScale;
} ArcadeKartWheelConfig;

typedef struct {
    f32 gripScale;
    f32 rollingResistanceScale;
    f32 brakeScale;
    f32 roughness;
} ArcadeKartSurfaceConfig;

typedef struct {
    f32 steerYawGain;
    f32 steerYawGainDrift;
    f32 steerAuthorityLowSpeed;
    f32 steerAuthorityHighSpeed;
    f32 steerAuthorityBlendSpeed;
    f32 driftChargeRate;
    f32 driftChargeBySteer;
    f32 driftChargeBySlip;
    f32 miniTurboThresholds[ARCADE_KART_MINI_TURBO_STAGE_COUNT];
    f32 driftMinSpeed;
} ArcadeKartDriftConfig;

typedef struct {
    f32 steerAuthority;
    f32 yawAuthority;
    f32 gravityPerFrame;
    f32 lateralDamping;
} ArcadeKartAirConfig;

typedef struct {
    f32 miniTurboDurations[ARCADE_KART_MINI_TURBO_STAGE_COUNT];
    f32 miniTurboPowers[ARCADE_KART_MINI_TURBO_STAGE_COUNT];
    f32 microBoostPower;
    f32 microBoostDuration;
    f32 microBoostCooldown;
    f32 doubleTapWindow;
} ArcadeKartBoostConfig;

typedef struct {
    f32 accelScale;
    f32 topSpeedScale;
    f32 steerScale;
    f32 stabilityScale;
    f32 driftScale;
    f32 airborneScale;
    f32 massScale;
} ArcadeKartClassTuning;

typedef struct {
    ArcadeKartEngineConfig engine;
    ArcadeKartTransmissionConfig transmission;
    ArcadeKartDifferentialConfig differential;
    ArcadeKartWheelConfig wheel;
    ArcadeKartDriftConfig drift;
    ArcadeKartAirConfig air;
    ArcadeKartBoostConfig boost;
    ArcadeKartSurfaceConfig surfaces[ARCADE_KART_SURFACE_COUNT];
    ArcadeKartClassTuning classes[ARCADE_KART_CLASS_COUNT];
    f32 baseMass;
    f32 baseTopSpeed;
    f32 reverseTopSpeed;
    f32 accelForceScale;
    f32 aeroDrag;
    f32 rollingDrag;
    f32 offThrottleDecel;
    f32 brakeForceScale;
} ArcadeKartConfig;

void arcade_kart_config_set_defaults(ArcadeKartConfig* config);
ArcadeKartClassType arcade_kart_character_class_for_character(s32 characterId);
void arcade_kart_config_apply_class(ArcadeKartConfig* outConfig, const ArcadeKartConfig* baseConfig,
                                    ArcadeKartClassType classType);

#endif
