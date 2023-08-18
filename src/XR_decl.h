#pragma once
// The *_decl files are really part of their main files, just including
// some declarations for things because the C++ file dependency system sucks.
#define XR_USE_GRAPHICS_API_VULKAN
// Normally openxr doesn't have XR_NO_STDDEF_H, I added it to match the vulkan header so I'm not forced to include crt stuff
#define XR_NO_PROTOTYPES
#define XR_NO_STDINT_H
#define XR_NO_STDDEF_H
#pragma warning(push, 0)
#include "..\external\openxr\openxr.h"
#include "..\external\openxr\openxr_platform.h"
#pragma warning(pop)

#define CHK_XR(cmd) { XrResult chkXr_Result = (cmd); if(XR_FAILED(chkXr_Result)){ ::XR::openxr_failure(chkXr_Result); } if(chkXr_Result == XR_SESSION_LOSS_PENDING){ abort("XR Session Loss Pending"); } } 
#define XR_DEBUG 1
namespace XR {
#define OPENXR_SWAPCHAIN_IMAGE_FORMAT VK_FORMAT_R8G8B8A8_SRGB
// Preprocessor hack to make defining and loading functions easier (just add them to the end of this macro when needed)
#define XR_FUNCTIONS OP(xrDestroyInstance)\
	OP(xrCreateDebugUtilsMessengerEXT)\
	OP(xrDestroyDebugUtilsMessengerEXT)\
	OP(xrEnumerateSwapchainImages)\
	OP(xrGetVulkanGraphicsDevice2KHR)\
	OP(xrGetSystem)\
	OP(xrCreateVulkanInstanceKHR)\
	OP(xrCreateVulkanDeviceKHR)\
	OP(xrCreateSession)\
	OP(xrGetVulkanGraphicsRequirements2KHR)\
	OP(xrEnumerateSwapchainFormats)\
	OP(xrPollEvent)\
	OP(xrBeginSession)\
	OP(xrEndSession)\
	OP(xrDestroySession)\
	OP(xrWaitFrame)\
	OP(xrBeginFrame)\
	OP(xrEndFrame)\
	OP(xrCreateSwapchain)\
	OP(xrDestroySwapchain)\
	OP(xrAcquireSwapchainImage)\
	OP(xrReleaseSwapchainImage)\
	OP(xrWaitSwapchainImage)\
	OP(xrGetSystemProperties)\
	OP(xrEnumerateViewConfigurationViews)\
	OP(xrEnumerateReferenceSpaces)\
	OP(xrCreateReferenceSpace)\
	OP(xrLocateViews)

#define OP(func) extern PFN_##func func;
XR_FUNCTIONS
#undef OP

static constexpr XrPosef IDENTITY_POSE{ XrQuaternionf{ 0.0F, 0.0F, 0.0F, 1.0F }, XrVector3f{ 0.0F, 0.0F, 0.0F } };

struct OpenXRFrameInfo {
	XrPosef leftEyePose;
	XrFovf leftEyeFov;
	XrPosef rightEyePose;
	XrFovf rightEyeFov;
	XrTime predictedDisplayTime;
	b32 shouldRender;
};

extern XrInstance xrInstance;
extern XrSystemId systemID;
extern XrSession session;
extern XrSessionState currentSessionState;

extern u32 xrRenderWidth;
extern u32 xrRenderHeight;

void openxr_failure(XrResult result);
void end_openxr();
Quaternionf xr_quat_to_drillmath_quat(XrQuaternionf q);
Vector3f xr_vec3_to_drillmath_vec3(XrVector3f v);

}