#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum VRAvailability {
    VR_AVAILABILITY_NOT_INITIALIZED = 0,
    VR_AVAILABILITY_DISABLED = 1,
    VR_AVAILABILITY_NOT_COMPILED = 2,
    VR_AVAILABILITY_NO_RUNTIME = 3,
    VR_AVAILABILITY_NO_HMD = 4,
    VR_AVAILABILITY_HMD_AVAILABLE = 5,
} VRAvailability;

void VR_Init(void);
void VR_Shutdown(void);
void VR_BeginFrame(void);
void VR_RenderFrame(void);
int VR_IsHmdAvailable(void);
int VR_IsStereoRenderTargetReady(void);
VRAvailability VR_GetAvailability(void);

#ifdef __cplusplus
}
#endif
