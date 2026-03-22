#ifndef ARCADE_KART_TYPES_H
#define ARCADE_KART_TYPES_H

#include <common_structs.h>
#include <stdbool.h>

#define ARCADE_KART_MAX_GEARS 6
#define ARCADE_KART_WHEEL_COUNT 4
#define ARCADE_KART_MINI_TURBO_STAGE_COUNT 3

typedef enum {
    ARCADE_KART_WHEEL_FRONT_LEFT = 0,
    ARCADE_KART_WHEEL_FRONT_RIGHT,
    ARCADE_KART_WHEEL_REAR_LEFT,
    ARCADE_KART_WHEEL_REAR_RIGHT
} ArcadeKartWheelPosition;

typedef enum {
    ARCADE_KART_TRANSMISSION_AUTOMATIC = 0,
    ARCADE_KART_TRANSMISSION_MANUAL
} ArcadeKartTransmissionMode;

typedef enum {
    ARCADE_KART_DIFF_OPEN = 0,
    ARCADE_KART_DIFF_SPOOL,
    ARCADE_KART_DIFF_LSD
} ArcadeKartDifferentialType;

typedef enum {
    ARCADE_KART_SURFACE_ROAD = 0,
    ARCADE_KART_SURFACE_DIRT,
    ARCADE_KART_SURFACE_GRASS,
    ARCADE_KART_SURFACE_SAND,
    ARCADE_KART_SURFACE_ICE,
    ARCADE_KART_SURFACE_AIRBORNE,
    ARCADE_KART_SURFACE_COUNT
} ArcadeKartSurfaceType;

typedef enum {
    ARCADE_KART_CLASS_LIGHT = 0,
    ARCADE_KART_CLASS_MEDIUM,
    ARCADE_KART_CLASS_HEAVY,
    ARCADE_KART_CLASS_COUNT
} ArcadeKartClassType;

typedef struct {
    f32 throttle;
    f32 brake;
    f32 clutch;
    f32 steer;
    bool driftHeld;
    bool throttlePressed;
    bool shiftUpPressed;
    bool shiftDownPressed;
    s8 requestedGear;
    ArcadeKartTransmissionMode transmissionMode;
} ArcadeKartInput;

typedef struct {
    f32 rpm;
    f32 throttleFiltered;
    f32 lastDriveTorque;
    f32 lastEngineBrakeTorque;
} ArcadeKartEngineState;

typedef struct {
    ArcadeKartTransmissionMode mode;
    s8 currentGear;
    s8 targetGear;
    f32 shiftTimer;
    f32 perfectShiftTimer;
    f32 missedShiftTimer;
} ArcadeKartTransmissionState;

typedef struct {
    f32 lockAmount;
} ArcadeKartDifferentialState;

typedef struct {
    bool inContact;
    ArcadeKartSurfaceType surfaceType;
    f32 angularSpeed;
    f32 normalLoad;
    f32 longitudinalSlip;
    f32 lateralSlip;
    f32 grip;
    f32 driveTorque;
    f32 brakeTorque;
} ArcadeKartWheelState;

typedef struct {
    f32 charge;
    s8 stage;
    f32 boostTimer;
    f32 boostPower;
} ArcadeKartMiniTurboState;

typedef struct {
    f32 tapWindowTimer;
    f32 cooldownTimer;
    f32 boostTimer;
    bool triggeredThisFrame;
} ArcadeKartMicroBoostState;

typedef struct {
    f32 surfaceRoughness;
    f32 slipIntensity;
    f32 loadCue;
    f32 steeringResistance;
    f32 airborneHint;
    f32 drivetrainVibe;
} ArcadeKartForceFeedbackSignals;

typedef struct {
    ArcadeKartEngineState engine;
    ArcadeKartTransmissionState transmission;
    ArcadeKartDifferentialState differential;
    ArcadeKartWheelState wheels[ARCADE_KART_WHEEL_COUNT];
    ArcadeKartMiniTurboState miniTurbo;
    ArcadeKartMicroBoostState microBoost;
    ArcadeKartForceFeedbackSignals ffb;
    f32 forwardSpeed;
    f32 lateralSpeed;
    f32 yawVelocity;
    f32 airborneTimer;
    f32 lastThrottleInput;
    f32 lastSteerInput;
} ArcadeKartPhysicsState;

typedef struct {
    ArcadeKartSurfaceType wheelSurface[ARCADE_KART_WHEEL_COUNT];
    bool wheelContact[ARCADE_KART_WHEEL_COUNT];
} ArcadeKartEnvironment;

typedef struct {
    f32 forwardSpeed;
    f32 lateralSpeed;
    f32 yawDelta;
    f32 longitudinalAccel;
    f32 lateralAccel;
    f32 engineRpm;
    f32 driveTorque;
    f32 boostPower;
    f32 wheelTorque[ARCADE_KART_WHEEL_COUNT];
    f32 wheelBrakeTorque[ARCADE_KART_WHEEL_COUNT];
    s8 miniTurboStage;
    bool miniTurboTriggered;
    bool microBoostTriggered;
    bool airborne;
    ArcadeKartForceFeedbackSignals ffb;
} ArcadeKartPhysicsOutput;

#endif
