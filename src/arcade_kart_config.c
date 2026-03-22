#include "arcade_kart_config.h"

#include <defines.h>
#include <macros.h>

void arcade_kart_config_set_defaults(ArcadeKartConfig* config) {
    if (config == NULL) {
        return;
    }

    *config = (ArcadeKartConfig){
        .engine =
            {
                .idleRpm = 1350.0f,
                .redlineRpm = 9600.0f,
                .maxRpm = 10400.0f,
                .throttleResponse = 11.0f,
                .rpmResponse = 8.5f,
                .peakTorque = 410.0f,
                .lowEndTorqueBoost = 1.22f,
                .highRpmTorqueDropoff = 0.24f,
                .engineBrakeTorque = 150.0f,
            },
        .transmission =
            {
                .gearRatios = { 2.92f, 2.21f, 1.67f, 1.29f, 1.02f, 0.85f },
                .maxForwardGear = 6,
                .reverseRatio = 2.45f,
                .finalDrive = 2.85f,
                .upshiftRpm = 9000.0f,
                .downshiftRpm = 5200.0f,
                .shiftLatencySeconds = 0.115f,
                .autoTorqueScale = 0.97f,
                .perfectShiftWindowRpm = 430.0f,
                .perfectShiftBoostDuration = 0.33f,
                .manualPerfectShiftTorqueScale = 1.07f,
            },
        .differential =
            {
                .type = ARCADE_KART_DIFF_LSD,
                .frontTorqueSplit = 0.0f,
                .lockStrength = 0.50f,
                .lsdStrength = 0.72f,
            },
        .wheel =
            {
                .wheelRadius = 0.34f,
                .wheelInertia = 0.95f,
                .baseLongGrip = 1.22f,
                .baseLatGrip = 1.15f,
                .rollingResistance = 0.028f,
                .lateralDamping = 0.36f,
                .maxBrakeTorque = 620.0f,
                .driftRearLateralGripScale = 0.74f,
                .slipToFfbScale = 1.0f,
            },
        .drift =
            {
                .steerYawGain = 3.15f,
                .steerYawGainDrift = 4.05f,
                .steerAuthorityLowSpeed = 1.18f,
                .steerAuthorityHighSpeed = 0.58f,
                .steerAuthorityBlendSpeed = 8.6f,
                .driftChargeRate = 0.85f,
                .driftChargeBySteer = 0.75f,
                .driftChargeBySlip = 0.9f,
                .miniTurboThresholds = { 1.05f, 2.25f, 3.70f },
                .driftMinSpeed = 2.15f,
            },
        .air =
            {
                .steerAuthority = 1.05f,
                .yawAuthority = 1.25f,
                .gravityPerFrame = 0.26f,
                .lateralDamping = 0.965f,
            },
        .boost =
            {
                .miniTurboDurations = { 0.65f, 1.05f, 1.45f },
                .miniTurboPowers = { 0.48f, 0.72f, 0.95f },
                .microBoostPower = 0.35f,
                .microBoostDuration = 0.26f,
                .microBoostCooldown = 1.65f,
                .doubleTapWindow = 0.28f,
            },
        .surfaces =
            {
                [ARCADE_KART_SURFACE_ROAD] =
                    {
                        .gripScale = 1.0f,
                        .rollingResistanceScale = 1.0f,
                        .brakeScale = 1.0f,
                        .roughness = 0.10f,
                    },
                [ARCADE_KART_SURFACE_DIRT] =
                    {
                        .gripScale = 0.86f,
                        .rollingResistanceScale = 1.26f,
                        .brakeScale = 0.90f,
                        .roughness = 0.55f,
                    },
                [ARCADE_KART_SURFACE_GRASS] =
                    {
                        .gripScale = 0.70f,
                        .rollingResistanceScale = 1.65f,
                        .brakeScale = 0.84f,
                        .roughness = 0.68f,
                    },
                [ARCADE_KART_SURFACE_SAND] =
                    {
                        .gripScale = 0.74f,
                        .rollingResistanceScale = 1.42f,
                        .brakeScale = 0.86f,
                        .roughness = 0.62f,
                    },
                [ARCADE_KART_SURFACE_ICE] =
                    {
                        .gripScale = 0.50f,
                        .rollingResistanceScale = 0.80f,
                        .brakeScale = 0.48f,
                        .roughness = 0.07f,
                    },
                [ARCADE_KART_SURFACE_AIRBORNE] =
                    {
                        .gripScale = 0.0f,
                        .rollingResistanceScale = 0.0f,
                        .brakeScale = 0.15f,
                        .roughness = 0.0f,
                    },
            },
        .classes =
            {
                [ARCADE_KART_CLASS_LIGHT] =
                    {
                        .accelScale = 1.11f,
                        .topSpeedScale = 0.94f,
                        .steerScale = 1.12f,
                        .stabilityScale = 0.86f,
                        .driftScale = 1.12f,
                        .airborneScale = 1.08f,
                        .massScale = 0.88f,
                    },
                [ARCADE_KART_CLASS_MEDIUM] =
                    {
                        .accelScale = 1.0f,
                        .topSpeedScale = 1.0f,
                        .steerScale = 1.0f,
                        .stabilityScale = 1.0f,
                        .driftScale = 1.0f,
                        .airborneScale = 1.0f,
                        .massScale = 1.0f,
                    },
                [ARCADE_KART_CLASS_HEAVY] =
                    {
                        .accelScale = 0.89f,
                        .topSpeedScale = 1.08f,
                        .steerScale = 0.90f,
                        .stabilityScale = 1.16f,
                        .driftScale = 0.90f,
                        .airborneScale = 0.90f,
                        .massScale = 1.20f,
                    },
            },
        .baseMass = 150.0f,
        .baseTopSpeed = 9.8f,
        .reverseTopSpeed = 4.4f,
        .accelForceScale = 0.0105f,
        .aeroDrag = 0.034f,
        .rollingDrag = 0.090f,
        .offThrottleDecel = 0.11f,
        .brakeForceScale = 8.4f,
    };
}

ArcadeKartClassType arcade_kart_character_class_for_character(s32 characterId) {
    switch (characterId) {
        case YOSHI:
        case TOAD:
        case PEACH:
            return ARCADE_KART_CLASS_LIGHT;
        case DK:
        case WARIO:
        case BOWSER:
            return ARCADE_KART_CLASS_HEAVY;
        case MARIO:
        case LUIGI:
        default:
            return ARCADE_KART_CLASS_MEDIUM;
    }
}

void arcade_kart_config_apply_class(ArcadeKartConfig* outConfig, const ArcadeKartConfig* baseConfig,
                                    ArcadeKartClassType classType) {
    ArcadeKartClassTuning tuning;

    if (outConfig == NULL || baseConfig == NULL) {
        return;
    }

    *outConfig = *baseConfig;
    if (classType < 0 || classType >= ARCADE_KART_CLASS_COUNT) {
        classType = ARCADE_KART_CLASS_MEDIUM;
    }

    tuning = baseConfig->classes[classType];

    outConfig->baseMass *= tuning.massScale;
    outConfig->baseTopSpeed *= tuning.topSpeedScale;
    outConfig->reverseTopSpeed *= (0.96f + (tuning.topSpeedScale * 0.04f));
    outConfig->accelForceScale *= tuning.accelScale;
    outConfig->engine.peakTorque *= tuning.accelScale;
    outConfig->drift.steerYawGain *= tuning.steerScale;
    outConfig->drift.steerYawGainDrift *= tuning.steerScale;
    outConfig->drift.driftChargeRate *= tuning.driftScale;
    outConfig->drift.driftChargeBySteer *= tuning.driftScale;
    outConfig->air.steerAuthority *= tuning.airborneScale;
    outConfig->air.yawAuthority *= tuning.airborneScale;
    outConfig->wheel.baseLatGrip *= tuning.stabilityScale;
    outConfig->wheel.baseLongGrip *= (0.9f + (tuning.stabilityScale * 0.1f));
}
