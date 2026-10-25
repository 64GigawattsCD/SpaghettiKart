#include "vr/openxr_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <libultraship.h>
#include <spdlog/spdlog.h>

extern "C" {
#include "camera.h"
#include "defines.h"
#include "main.h"
}

#if SPAGHETTIKART_OPENXR
#include <algorithm>
#include <vector>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <d3d11.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#endif

extern "C" int gfx_create_framebuffer(uint32_t width, uint32_t height, uint32_t native_width, uint32_t native_height,
                                      uint8_t resize);
extern "C" int gfx_dx11_get_device_context(void** device, void** context);
extern "C" int gfx_dx11_copy_framebuffer_to_texture(int framebufferId, void* targetTexture, uint32_t* copiedWidth,
                                                    uint32_t* copiedHeight);

namespace {

struct VrEyeTarget {
    int framebufferId = -1;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct VrRuntimeState {
    bool initialized = false;
    bool hmdAvailable = false;
    bool stereoRenderTargetReady = false;
    bool printedStatus = false;
    VRAvailability availability = VR_AVAILABILITY_NOT_INITIALIZED;
    int32_t lastXrResult = 0;
    uint32_t viewCount = 0;
    VrEyeTarget eyes[2];
#if SPAGHETTIKART_OPENXR
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace viewSpace = XR_NULL_HANDLE;
    XrSpace localSpace = XR_NULL_HANDLE;
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    bool sessionRunning = false;
    bool frameBegun = false;
    bool copiedGameFramebuffer = false;
    bool submittedMenuQuad = false;
    bool menuPoseInitialized = false;
    XrPosef menuPose = {};
    uint32_t copiedWidth[2] = {};
    uint32_t copiedHeight[2] = {};
    XrSwapchain swapchains[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
    std::vector<XrSwapchainImageD3D11KHR> swapchainImages[2];
    XrView views[2] = {};
    XrCompositionLayerProjectionView projectionViews[2] = {};
    XrViewConfigurationView viewConfigs[2] = {};
    DXGI_FORMAT swapchainFormat = DXGI_FORMAT_UNKNOWN;
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
#endif
};

VrRuntimeState sVr;

void VR_Log(const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    SPDLOG_INFO("[VR/OpenXR] {}", message);

    FILE* file = fopen("vr_openxr.log", "a");
    if (file != nullptr) {
        fprintf(file, "[VR/OpenXR] %s\n", message);
        fclose(file);
    }
}

void VR_PublishState(void) {
    CVarSetInteger("gArcadeKart.VR.OpenXR.Compiled", SPAGHETTIKART_OPENXR ? 1 : 0);
    CVarSetInteger("gArcadeKart.VR.Availability", sVr.availability);
    CVarSetInteger("gArcadeKart.VR.HmdAvailable", sVr.hmdAvailable ? 1 : 0);
    CVarSetInteger("gArcadeKart.VR.StereoRenderTargetReady", sVr.stereoRenderTargetReady ? 1 : 0);
    CVarSetInteger("gArcadeKart.VR.LastXrResult", sVr.lastXrResult);
    CVarSetInteger("gArcadeKart.VR.ViewCount", sVr.viewCount);
    CVarSetInteger("gArcadeKart.VR.LeftEyeFramebufferId", sVr.eyes[0].framebufferId);
    CVarSetInteger("gArcadeKart.VR.RightEyeFramebufferId", sVr.eyes[1].framebufferId);
    CVarSetInteger("gArcadeKart.VR.LeftEyeWidth", sVr.eyes[0].width);
    CVarSetInteger("gArcadeKart.VR.LeftEyeHeight", sVr.eyes[0].height);
    CVarSetInteger("gArcadeKart.VR.RightEyeWidth", sVr.eyes[1].width);
    CVarSetInteger("gArcadeKart.VR.RightEyeHeight", sVr.eyes[1].height);
    CVarRegisterInteger("gArcadeKart.VR.DisableMenuPreview", 1);
    CVarRegisterFloat("gArcadeKart.VR.MenuQuadWidthMeters", 1.8f);
    CVarRegisterFloat("gArcadeKart.VR.MenuQuadHeightMeters", 1.35f);
    CVarRegisterFloat("gArcadeKart.VR.MenuQuadDistanceMeters", 2.0f);
    CVarRegisterFloat("gArcadeKart.VR.MenuQuadVerticalOffsetMeters", -0.2f);
    CVarRegisterInteger("gArcadeKart.VR.MenuQuadSourceWidth", 0);
    CVarRegisterInteger("gArcadeKart.VR.MenuQuadSourceHeight", 0);
#if SPAGHETTIKART_OPENXR
    CVarSetInteger("gArcadeKart.VR.SessionCreated", sVr.session != XR_NULL_HANDLE ? 1 : 0);
    CVarSetInteger("gArcadeKart.VR.SessionRunning", sVr.sessionRunning ? 1 : 0);
    CVarSetInteger("gArcadeKart.VR.SessionState", sVr.sessionState);
#else
    CVarSetInteger("gArcadeKart.VR.SessionCreated", 0);
    CVarSetInteger("gArcadeKart.VR.SessionRunning", 0);
    CVarSetInteger("gArcadeKart.VR.SessionState", 0);
#endif
}

void VR_PrintStatusOnce(const char* message) {
    if (!sVr.printedStatus) {
        VR_Log("%s", message);
        sVr.printedStatus = true;
    }
}

void VR_CreateEyeFramebuffers(void) {
    if (!sVr.hmdAvailable || sVr.stereoRenderTargetReady || sVr.viewCount < 2) {
        return;
    }

    for (uint32_t eye = 0; eye < 2; eye++) {
        VrEyeTarget* target = &sVr.eyes[eye];

        if ((target->width == 0) || (target->height == 0)) {
            target->width = 1024;
            target->height = 1024;
        }

        if (target->framebufferId == -1) {
            target->framebufferId =
                gfx_create_framebuffer(target->width, target->height, target->width, target->height, true);
        }
    }

    sVr.stereoRenderTargetReady = (sVr.eyes[0].framebufferId != -1) && (sVr.eyes[1].framebufferId != -1);
    VR_PublishState();
}

#if SPAGHETTIKART_OPENXR
bool VR_HasInstanceExtension(const char* extensionName) {
    uint32_t extensionCount = 0;
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
    if (XR_FAILED(result)) {
        sVr.lastXrResult = result;
        return false;
    }

    std::vector<XrExtensionProperties> extensions(extensionCount);
    for (uint32_t i = 0; i < extensionCount; i++) {
        extensions[i].type = XR_TYPE_EXTENSION_PROPERTIES;
    }

    result = xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensions.data());
    if (XR_FAILED(result)) {
        sVr.lastXrResult = result;
        return false;
    }

    for (const XrExtensionProperties& extension : extensions) {
        if (strcmp(extension.extensionName, extensionName) == 0) {
            return true;
        }
    }
    return false;
}

int64_t VR_SelectSwapchainFormat(const std::vector<int64_t>& formats) {
    const int64_t preferredFormats[] = {
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    };

    for (int64_t preferred : preferredFormats) {
        if (std::find(formats.begin(), formats.end(), preferred) != formats.end()) {
            return preferred;
        }
    }

    return formats.empty() ? DXGI_FORMAT_R8G8B8A8_UNORM : formats[0];
}

bool VR_CreateSessionAndSwapchains(void) {
    PFN_xrGetD3D11GraphicsRequirementsKHR getD3D11Requirements = nullptr;
    XrGraphicsRequirementsD3D11KHR graphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
    XrGraphicsBindingD3D11KHR graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    XrSessionCreateInfo sessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
    XrReferenceSpaceCreateInfo spaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    uint32_t formatCount = 0;
    XrResult result;
    void* device = nullptr;
    void* context = nullptr;

    if (sVr.session != XR_NULL_HANDLE) {
        return true;
    }

    if (!gfx_dx11_get_device_context(&device, &context)) {
        CVarSetInteger("gArcadeKart.VR.Dx11DeviceAvailable", 0);
        return false;
    }
    CVarSetInteger("gArcadeKart.VR.Dx11DeviceAvailable", 1);

    sVr.d3dDevice = static_cast<ID3D11Device*>(device);
    sVr.d3dContext = static_cast<ID3D11DeviceContext*>(context);

    result = xrGetInstanceProcAddr(sVr.instance, "xrGetD3D11GraphicsRequirementsKHR",
                                   reinterpret_cast<PFN_xrVoidFunction*>(&getD3D11Requirements));
    sVr.lastXrResult = result;
    if (XR_FAILED(result) || getD3D11Requirements == nullptr) {
        VR_Log("xrGetD3D11GraphicsRequirementsKHR unavailable: %d", result);
        return false;
    }

    result = getD3D11Requirements(sVr.instance, sVr.systemId, &graphicsRequirements);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrGetD3D11GraphicsRequirementsKHR failed: %d", result);
        return false;
    }

    graphicsBinding.device = sVr.d3dDevice;
    sessionInfo.next = &graphicsBinding;
    sessionInfo.systemId = sVr.systemId;

    result = xrCreateSession(sVr.instance, &sessionInfo, &sVr.session);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrCreateSession failed: %d", result);
        return false;
    }
    VR_Log("Session created.");

    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    result = xrCreateReferenceSpace(sVr.session, &spaceInfo, &sVr.viewSpace);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrCreateReferenceSpace(VIEW) failed: %d", result);
        return false;
    }

    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    result = xrCreateReferenceSpace(sVr.session, &spaceInfo, &sVr.localSpace);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrCreateReferenceSpace(LOCAL) failed: %d", result);
        return false;
    }

    result = xrEnumerateSwapchainFormats(sVr.session, 0, &formatCount, nullptr);
    sVr.lastXrResult = result;
    if (XR_FAILED(result) || formatCount == 0) {
        VR_Log("xrEnumerateSwapchainFormats failed: %d count=%u", result, formatCount);
        return false;
    }

    std::vector<int64_t> formats(formatCount);
    result = xrEnumerateSwapchainFormats(sVr.session, formatCount, &formatCount, formats.data());
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("Failed reading swapchain formats: %d", result);
        return false;
    }

    const int64_t colorFormat = VR_SelectSwapchainFormat(formats);
    sVr.swapchainFormat = static_cast<DXGI_FORMAT>(colorFormat);
    CVarSetInteger("gArcadeKart.VR.SwapchainFormat", static_cast<int32_t>(colorFormat));
    VR_Log("Selected swapchain format %lld from %u available formats.", static_cast<long long>(colorFormat),
           formatCount);

    for (uint32_t eye = 0; eye < 2; eye++) {
        XrSwapchainCreateInfo swapchainInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
        uint32_t imageCount = 0;

        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swapchainInfo.format = colorFormat;
        swapchainInfo.sampleCount = sVr.viewConfigs[eye].recommendedSwapchainSampleCount;
        swapchainInfo.width = sVr.eyes[eye].width;
        swapchainInfo.height = sVr.eyes[eye].height;
        swapchainInfo.faceCount = 1;
        swapchainInfo.arraySize = 1;
        swapchainInfo.mipCount = 1;

        result = xrCreateSwapchain(sVr.session, &swapchainInfo, &sVr.swapchains[eye]);
        sVr.lastXrResult = result;
        if (XR_FAILED(result)) {
            VR_Log("xrCreateSwapchain eye %u failed: %d", eye, result);
            return false;
        }

        result = xrEnumerateSwapchainImages(sVr.swapchains[eye], 0, &imageCount, nullptr);
        sVr.lastXrResult = result;
        if (XR_FAILED(result) || imageCount == 0) {
            VR_Log("xrEnumerateSwapchainImages eye %u failed: %d count=%u", eye, result, imageCount);
            return false;
        }

        sVr.swapchainImages[eye].resize(imageCount);
        for (XrSwapchainImageD3D11KHR& image : sVr.swapchainImages[eye]) {
            image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
        }

        result = xrEnumerateSwapchainImages(sVr.swapchains[eye], imageCount, &imageCount,
                                            reinterpret_cast<XrSwapchainImageBaseHeader*>(
                                                sVr.swapchainImages[eye].data()));
        sVr.lastXrResult = result;
        if (XR_FAILED(result)) {
            VR_Log("Failed reading swapchain images eye %u: %d", eye, result);
            return false;
        }
        VR_Log("Swapchain eye %u ready: %ux%u images=%u.", eye, sVr.eyes[eye].width, sVr.eyes[eye].height,
               imageCount);
    }

    VR_Log("Session and stereo swapchains created.");
    VR_PublishState();
    return true;
}

void VR_PollEvents(void) {
    XrEventDataBuffer eventData = { XR_TYPE_EVENT_DATA_BUFFER };

    while (xrPollEvent(sVr.instance, &eventData) == XR_SUCCESS) {
        switch (eventData.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                const XrEventDataSessionStateChanged* stateEvent =
                    reinterpret_cast<const XrEventDataSessionStateChanged*>(&eventData);

                sVr.sessionState = stateEvent->state;
                CVarSetInteger("gArcadeKart.VR.SessionState", sVr.sessionState);
                VR_Log("Session state changed to %d.", stateEvent->state);

                if (stateEvent->state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo beginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
                    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    XrResult result = xrBeginSession(sVr.session, &beginInfo);
                    sVr.lastXrResult = result;
                    if (XR_SUCCEEDED(result)) {
                        sVr.sessionRunning = true;
                        VR_Log("Session running.");
                    } else {
                        VR_Log("xrBeginSession failed: %d", result);
                    }
                } else if (stateEvent->state == XR_SESSION_STATE_STOPPING) {
                    XrResult result = xrEndSession(sVr.session);
                    sVr.lastXrResult = result;
                    sVr.sessionRunning = false;
                    sVr.menuPoseInitialized = false;
                    VR_Log("Session stopping: %d", result);
                } else if ((stateEvent->state == XR_SESSION_STATE_EXITING) ||
                           (stateEvent->state == XR_SESSION_STATE_LOSS_PENDING)) {
                    sVr.sessionRunning = false;
                    sVr.menuPoseInitialized = false;
                }
                break;
            }
            default:
                break;
        }

        eventData = { XR_TYPE_EVENT_DATA_BUFFER };
    }

    VR_PublishState();
}

bool VR_RenderTestSwapchainEye(uint32_t eye, const XrView& view, XrCompositionLayerProjectionView* projectionView) {
    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    uint32_t imageIndex = 0;
    int sourceFramebuffer = 0;
    XrResult result;
    static bool loggedRenderTargetFailure[2] = {};

    result = xrAcquireSwapchainImage(sVr.swapchains[eye], &acquireInfo, &imageIndex);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrAcquireSwapchainImage eye %u failed: %d", eye, result);
        return false;
    }

    waitInfo.timeout = XR_INFINITE_DURATION;
    result = xrWaitSwapchainImage(sVr.swapchains[eye], &waitInfo);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrWaitSwapchainImage eye %u failed: %d", eye, result);
        return false;
    }

    const float leftColor[4] = { 0.45f, 0.02f, 0.02f, 1.0f };
    const float rightColor[4] = { 0.02f, 0.06f, 0.45f, 1.0f };
    ID3D11RenderTargetView* renderTarget = nullptr;
    D3D11_TEXTURE2D_DESC textureDesc = {};
    D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
    sVr.swapchainImages[eye][imageIndex].texture->GetDesc(&textureDesc);
    viewDesc.Format = sVr.swapchainFormat;
    viewDesc.ViewDimension =
        textureDesc.SampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
    HRESULT hr = sVr.d3dDevice->CreateRenderTargetView(sVr.swapchainImages[eye][imageIndex].texture, &viewDesc,
                                                       &renderTarget);
    if (renderTarget != nullptr) {
        sVr.d3dContext->ClearRenderTargetView(renderTarget, eye == 0 ? leftColor : rightColor);
        renderTarget->Release();
    } else {
        if (!loggedRenderTargetFailure[eye]) {
            VR_Log("CreateRenderTargetView eye %u failed: hr=0x%08X tex=%ux%u texFmt=%u rtvFmt=%u samples=%u bind=0x%X misc=0x%X",
                   eye, static_cast<unsigned int>(hr), textureDesc.Width, textureDesc.Height, textureDesc.Format,
                   viewDesc.Format, textureDesc.SampleDesc.Count, textureDesc.BindFlags, textureDesc.MiscFlags);
            loggedRenderTargetFailure[eye] = true;
        }
        xrReleaseSwapchainImage(sVr.swapchains[eye], &releaseInfo);
        return false;
    }

    CVarRegisterInteger("gArcadeKart.VR.UseSeparatedRaceWorld", 0);
    if ((gGamestate == RACING) && (CVarGetInteger("gArcadeKart.VR.UseSeparatedRaceWorld", 0) != 0) &&
        (CVarGetInteger("gArcadeKart.PostFx.LayeredHudActive", 0) != 0)) {
        sourceFramebuffer = CVarGetInteger("gArcadeKart.PostFx.SceneFramebufferId", 0);
    }
    CVarSetInteger("gArcadeKart.VR.WorldSourceFramebufferId", sourceFramebuffer);
    CVarSetInteger("gArcadeKart.VR.HudSourceFramebufferId", CVarGetInteger("gArcadeKart.PostFx.HudFramebufferId", -1));
    CVarSetInteger("gArcadeKart.VR.HudLayerAvailable", CVarGetInteger("gArcadeKart.PostFx.LayeredHudActive", 0));

    if (gfx_dx11_copy_framebuffer_to_texture(sourceFramebuffer, sVr.swapchainImages[eye][imageIndex].texture,
                                             &sVr.copiedWidth[eye], &sVr.copiedHeight[eye])) {
        CVarSetInteger("gArcadeKart.VR.GameFramebufferCopied", 1);
        CVarSetInteger(eye == 0 ? "gArcadeKart.VR.LeftCopiedWidth" : "gArcadeKart.VR.RightCopiedWidth",
                       sVr.copiedWidth[eye]);
        CVarSetInteger(eye == 0 ? "gArcadeKart.VR.LeftCopiedHeight" : "gArcadeKart.VR.RightCopiedHeight",
                       sVr.copiedHeight[eye]);
        if (!sVr.copiedGameFramebuffer) {
            VR_Log("Copied renderer framebuffer 0 into OpenXR eye textures.");
            sVr.copiedGameFramebuffer = true;
        }
    } else {
        CVarSetInteger("gArcadeKart.VR.GameFramebufferCopied", 0);
    }

    result = xrReleaseSwapchainImage(sVr.swapchains[eye], &releaseInfo);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrReleaseSwapchainImage eye %u failed: %d", eye, result);
        return false;
    }

    projectionView->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    projectionView->pose = view.pose;
    projectionView->fov = view.fov;
    projectionView->subImage.swapchain = sVr.swapchains[eye];
    projectionView->subImage.imageRect.offset = { 0, 0 };
    projectionView->subImage.imageRect.extent = {
        static_cast<int32_t>(sVr.copiedWidth[eye] != 0 ? sVr.copiedWidth[eye] : sVr.eyes[eye].width),
        static_cast<int32_t>(sVr.copiedHeight[eye] != 0 ? sVr.copiedHeight[eye] : sVr.eyes[eye].height)
    };
    return true;
}

XrVector3f VR_RotateVectorByQuaternion(const XrVector3f& vector, const XrQuaternionf& q) {
    const XrVector3f u = { q.x, q.y, q.z };
    const float dotUV = (u.x * vector.x) + (u.y * vector.y) + (u.z * vector.z);
    const float dotUU = (u.x * u.x) + (u.y * u.y) + (u.z * u.z);
    const XrVector3f crossUV = {
        (u.y * vector.z) - (u.z * vector.y),
        (u.z * vector.x) - (u.x * vector.z),
        (u.x * vector.y) - (u.y * vector.x),
    };

    return {
        (2.0f * dotUV * u.x) + ((q.w * q.w - dotUU) * vector.x) + (2.0f * q.w * crossUV.x),
        (2.0f * dotUV * u.y) + ((q.w * q.w - dotUU) * vector.y) + (2.0f * q.w * crossUV.y),
        (2.0f * dotUV * u.z) + ((q.w * q.w - dotUU) * vector.z) + (2.0f * q.w * crossUV.z),
    };
}

bool VR_CaptureMenuPose(XrTime displayTime) {
    XrViewLocateInfo locateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
    XrViewState viewState = { XR_TYPE_VIEW_STATE };
    XrView views[2] = {};
    uint32_t viewCount = 0;
    XrResult result;

    if (sVr.menuPoseInitialized) {
        return true;
    }

    for (uint32_t i = 0; i < 2; i++) {
        views[i].type = XR_TYPE_VIEW;
    }

    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locateInfo.displayTime = displayTime;
    locateInfo.space = sVr.localSpace;

    result = xrLocateViews(sVr.session, &locateInfo, &viewState, 2, &viewCount, views);
    sVr.lastXrResult = result;
    if (XR_FAILED(result) || viewCount < 2 || (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
        VR_Log("Menu pose capture skipped: locate=%d views=%u flags=0x%llx", result, viewCount,
               static_cast<unsigned long long>(viewState.viewStateFlags));
        return false;
    }

    const float distance = CVarGetFloat("gArcadeKart.VR.MenuQuadDistanceMeters", 2.0f);
    const float verticalOffset = CVarGetFloat("gArcadeKart.VR.MenuQuadVerticalOffsetMeters", -0.2f);

    sVr.menuPose.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
    sVr.menuPose.position.x = views[0].pose.position.x;
    sVr.menuPose.position.y = views[0].pose.position.y + verticalOffset;
    sVr.menuPose.position.z = views[0].pose.position.z - distance;
    sVr.menuPoseInitialized = true;
    VR_Log("Captured fixed menu quad pose at %.2f %.2f %.2f.", sVr.menuPose.position.x, sVr.menuPose.position.y,
           sVr.menuPose.position.z);
    return true;
}

void VR_RenderOpenXRFrame(void) {
    XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO };
    XrFrameState frameState = { XR_TYPE_FRAME_STATE };
    XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
    XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
    XrCompositionLayerProjection projectionLayer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    XrCompositionLayerQuad menuQuadLayer = { XR_TYPE_COMPOSITION_LAYER_QUAD };
    XrViewLocateInfo locateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
    XrViewState viewState = { XR_TYPE_VIEW_STATE };
    uint32_t viewCount = 0;
    const XrCompositionLayerBaseHeader* layers[1] = {};
    XrResult result;
    static bool loggedShouldNotRender = false;
    static bool loggedSkippedPattern = false;

    if ((sVr.session == XR_NULL_HANDLE) || !sVr.sessionRunning) {
        return;
    }

    result = xrWaitFrame(sVr.session, &waitInfo, &frameState);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrWaitFrame failed: %d", result);
        return;
    }

    result = xrBeginFrame(sVr.session, &beginInfo);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrBeginFrame failed: %d", result);
        return;
    }
    sVr.frameBegun = true;

    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

    if (frameState.shouldRender) {
        const bool isRaceMode = gGamestate == RACING;
        CVarSetInteger("gArcadeKart.VR.SubmissionMode", isRaceMode ? 2 : 1);

        if (!isRaceMode) {
            XrView dummyView = { XR_TYPE_VIEW };
            if (VR_RenderTestSwapchainEye(0, dummyView, &sVr.projectionViews[0]) &&
                VR_CaptureMenuPose(frameState.predictedDisplayTime)) {
                const int32_t configuredMenuWidth = CVarGetInteger("gArcadeKart.VR.MenuQuadSourceWidth", 0);
                const int32_t configuredMenuHeight = CVarGetInteger("gArcadeKart.VR.MenuQuadSourceHeight", 0);
                const int32_t menuWidth =
                    configuredMenuWidth > 0
                        ? configuredMenuWidth
                        : static_cast<int32_t>(sVr.copiedWidth[0] != 0 ? sVr.copiedWidth[0] : sVr.eyes[0].width);
                const int32_t menuHeight =
                    configuredMenuHeight > 0
                        ? configuredMenuHeight
                        : static_cast<int32_t>(sVr.copiedHeight[0] != 0 ? sVr.copiedHeight[0] : sVr.eyes[0].height);
                float quadWidth = CVarGetFloat("gArcadeKart.VR.MenuQuadWidthMeters", 1.8f);
                float quadHeight = CVarGetFloat("gArcadeKart.VR.MenuQuadHeightMeters", 1.35f);

                menuQuadLayer.space = sVr.localSpace;
                menuQuadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                menuQuadLayer.subImage.swapchain = sVr.swapchains[0];
                menuQuadLayer.subImage.imageRect.offset = { 0, 0 };
                menuQuadLayer.subImage.imageRect.extent = {
                    std::min(menuWidth, static_cast<int32_t>(sVr.eyes[0].width)),
                    std::min(menuHeight, static_cast<int32_t>(sVr.eyes[0].height))
                };
                menuQuadLayer.pose = sVr.menuPose;
                menuQuadLayer.size.width = quadWidth;
                menuQuadLayer.size.height = quadHeight;
                layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&menuQuadLayer);
                endInfo.layerCount = 1;
                endInfo.layers = layers;
                CVarSetInteger("gArcadeKart.VR.MenuQuadSubmitted", 1);
                if (!sVr.submittedMenuQuad) {
                    VR_Log("Submitted menu quad: extent=%dx%d size=%.2fx%.2fm.", menuQuadLayer.subImage.imageRect.extent.width,
                           menuQuadLayer.subImage.imageRect.extent.height, quadWidth, quadHeight);
                    sVr.submittedMenuQuad = true;
                }
            } else {
                CVarSetInteger("gArcadeKart.VR.MenuQuadSubmitted", 0);
            }
        } else {
        sVr.menuPoseInitialized = false;
        for (uint32_t i = 0; i < 2; i++) {
            sVr.views[i].type = XR_TYPE_VIEW;
        }

        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.displayTime = frameState.predictedDisplayTime;
        locateInfo.space = sVr.viewSpace;

        result = xrLocateViews(sVr.session, &locateInfo, &viewState, 2, &viewCount, sVr.views);
        sVr.lastXrResult = result;
        if (XR_SUCCEEDED(result) && viewCount >= 2 &&
            (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0 &&
            VR_RenderTestSwapchainEye(0, sVr.views[0], &sVr.projectionViews[0]) &&
            VR_RenderTestSwapchainEye(1, sVr.views[1], &sVr.projectionViews[1])) {
            projectionLayer.space = sVr.viewSpace;
            projectionLayer.viewCount = 2;
            projectionLayer.views = sVr.projectionViews;
            layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer);
            endInfo.layerCount = 1;
            endInfo.layers = layers;
            CVarSetInteger("gArcadeKart.VR.TestPatternSubmitted", 1);
        } else {
            if (!loggedSkippedPattern) {
                VR_Log("Test pattern skipped: locate=%d views=%u flags=0x%llx", result, viewCount,
                       static_cast<unsigned long long>(viewState.viewStateFlags));
                loggedSkippedPattern = true;
            }
            CVarSetInteger("gArcadeKart.VR.TestPatternSubmitted", 0);
        }
        }
    } else {
        if (!loggedShouldNotRender) {
            VR_Log("xrWaitFrame returned shouldRender=false.");
            loggedShouldNotRender = true;
        }
    }

    result = xrEndFrame(sVr.session, &endInfo);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        VR_Log("xrEndFrame failed: %d", result);
    } else if (endInfo.layerCount > 0) {
        static bool loggedFirstSubmit = false;
        if (!loggedFirstSubmit) {
            VR_Log("Submitted first stereo test pattern frame.");
            loggedFirstSubmit = true;
        }
    }
    sVr.frameBegun = false;
}

bool VR_InitOpenXR(void) {
    XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
    uint32_t viewCount = 0;
    XrResult result;
    const char* extensions[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };

    if (!VR_HasInstanceExtension(XR_KHR_D3D11_ENABLE_EXTENSION_NAME)) {
        sVr.availability = VR_AVAILABILITY_NO_RUNTIME;
        VR_PrintStatusOnce("OpenXR runtime does not expose XR_KHR_D3D11_enable.");
        return false;
    }

    strncpy(createInfo.applicationInfo.applicationName, "SpaghettiKart", XR_MAX_APPLICATION_NAME_SIZE - 1);
    createInfo.applicationInfo.applicationVersion = 1;
    strncpy(createInfo.applicationInfo.engineName, "libultraship", XR_MAX_ENGINE_NAME_SIZE - 1);
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    createInfo.enabledExtensionCount = 1;
    createInfo.enabledExtensionNames = extensions;

    result = xrCreateInstance(&createInfo, &sVr.instance);
    sVr.lastXrResult = result;
    if (result == XR_ERROR_RUNTIME_UNAVAILABLE) {
        sVr.availability = VR_AVAILABILITY_NO_RUNTIME;
        VR_PrintStatusOnce("No OpenXR runtime is active.");
        return false;
    }
    if (XR_FAILED(result)) {
        sVr.availability = VR_AVAILABILITY_NO_RUNTIME;
        VR_Log("xrCreateInstance failed: %d", result);
        return false;
    }
    VR_Log("OpenXR instance created.");

    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    result = xrGetSystem(sVr.instance, &systemInfo, &sVr.systemId);
    sVr.lastXrResult = result;
    if (result == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
        sVr.availability = VR_AVAILABILITY_NO_HMD;
        VR_PrintStatusOnce("OpenXR runtime is present, but no HMD is available.");
        return false;
    }
    if (XR_FAILED(result)) {
        sVr.availability = VR_AVAILABILITY_NO_HMD;
        VR_Log("xrGetSystem failed: %d", result);
        return false;
    }
    VR_Log("OpenXR HMD system acquired: %llu.", static_cast<unsigned long long>(sVr.systemId));

    result = xrEnumerateViewConfigurationViews(sVr.instance, sVr.systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                                               &viewCount, nullptr);
    sVr.lastXrResult = result;
    if (XR_FAILED(result) || viewCount < 2) {
        sVr.availability = VR_AVAILABILITY_NO_HMD;
        VR_Log("Stereo view configuration unavailable: result=%d, views=%u", result, viewCount);
        return false;
    }

    sVr.viewCount = std::min<uint32_t>(viewCount, 2);
    for (uint32_t i = 0; i < sVr.viewCount; i++) {
        sVr.viewConfigs[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    }
    result = xrEnumerateViewConfigurationViews(sVr.instance, sVr.systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                               sVr.viewCount, &viewCount, sVr.viewConfigs);
    sVr.lastXrResult = result;
    if (XR_FAILED(result)) {
        sVr.availability = VR_AVAILABILITY_NO_HMD;
        VR_Log("Failed to read stereo view configuration: %d", result);
        return false;
    }

    for (uint32_t i = 0; i < sVr.viewCount; i++) {
        sVr.eyes[i].width = sVr.viewConfigs[i].recommendedImageRectWidth;
        sVr.eyes[i].height = sVr.viewConfigs[i].recommendedImageRectHeight;
    }

    sVr.hmdAvailable = true;
    sVr.availability = VR_AVAILABILITY_HMD_AVAILABLE;
    VR_Log("HMD available. Recommended eye sizes: L=%ux%u R=%ux%u", sVr.eyes[0].width, sVr.eyes[0].height,
           sVr.eyes[1].width, sVr.eyes[1].height);
    VR_CreateSessionAndSwapchains();
    return true;
}
#endif

} // namespace

extern "C" void VR_Init(void) {
    if (sVr.initialized) {
        return;
    }

    sVr.initialized = true;

    if (CVarGetInteger("gArcadeKart.VR.Enabled", 1) == 0) {
        sVr.availability = VR_AVAILABILITY_DISABLED;
        VR_PublishState();
        VR_PrintStatusOnce("VR disabled by gArcadeKart.VR.Enabled.");
        return;
    }

#if SPAGHETTIKART_OPENXR
    VR_InitOpenXR();
#else
    sVr.availability = VR_AVAILABILITY_NOT_COMPILED;
    VR_PrintStatusOnce("OpenXR loader was not available at build time.");
#endif

    VR_PublishState();
}

extern "C" void VR_Shutdown(void) {
#if SPAGHETTIKART_OPENXR
    for (uint32_t eye = 0; eye < 2; eye++) {
        if (sVr.swapchains[eye] != XR_NULL_HANDLE) {
            xrDestroySwapchain(sVr.swapchains[eye]);
            sVr.swapchains[eye] = XR_NULL_HANDLE;
        }
        sVr.swapchainImages[eye].clear();
    }
    if (sVr.viewSpace != XR_NULL_HANDLE) {
        xrDestroySpace(sVr.viewSpace);
        sVr.viewSpace = XR_NULL_HANDLE;
    }
    if (sVr.localSpace != XR_NULL_HANDLE) {
        xrDestroySpace(sVr.localSpace);
        sVr.localSpace = XR_NULL_HANDLE;
    }
    if (sVr.session != XR_NULL_HANDLE) {
        xrDestroySession(sVr.session);
        sVr.session = XR_NULL_HANDLE;
    }
    if (sVr.instance != XR_NULL_HANDLE) {
        xrDestroyInstance(sVr.instance);
        sVr.instance = XR_NULL_HANDLE;
    }
#endif
    sVr.initialized = false;
    sVr.hmdAvailable = false;
    sVr.stereoRenderTargetReady = false;
    sVr.availability = VR_AVAILABILITY_NOT_INITIALIZED;
    VR_PublishState();
}

extern "C" void VR_BeginFrame(void) {
    if (!sVr.initialized) {
        VR_Init();
    }

#if SPAGHETTIKART_OPENXR
    if (sVr.hmdAvailable && (sVr.session == XR_NULL_HANDLE)) {
        VR_CreateSessionAndSwapchains();
    }
    if (sVr.session != XR_NULL_HANDLE) {
        VR_PollEvents();
    }
#endif

    CVarSetInteger("gArcadeKart.VR.RenderRequested", sVr.hmdAvailable ? 1 : 0);
    CVarSetInteger("gArcadeKart.VR.MenuQuadActive", sVr.hmdAvailable && (gGamestate != RACING) ? 1 : 0);
    if (sVr.hmdAvailable && (gGamestate == RACING) && (CVarGetInteger("gArcadeKart.VR.AutoFirstPerson", 1) != 0)) {
        CM_EnableFirstPersonView(0);
        CVarSetInteger("gArcadeKart.VR.RaceCameraPathActive", 1);
    } else {
        CVarSetInteger("gArcadeKart.VR.RaceCameraPathActive", 0);
    }
}

extern "C" void VR_RenderFrame(void) {
    if (!sVr.hmdAvailable) {
        CVarSetInteger("gArcadeKart.VR.RenderActive", 0);
        return;
    }

    VR_CreateEyeFramebuffers();
#if SPAGHETTIKART_OPENXR
    VR_RenderOpenXRFrame();
#endif
    CVarSetInteger("gArcadeKart.VR.RenderActive", sVr.stereoRenderTargetReady ? 1 : 0);
}

extern "C" int VR_IsHmdAvailable(void) {
    return sVr.hmdAvailable ? 1 : 0;
}

extern "C" int VR_IsStereoRenderTargetReady(void) {
    return sVr.stereoRenderTargetReady ? 1 : 0;
}

extern "C" VRAvailability VR_GetAvailability(void) {
    return sVr.availability;
}
