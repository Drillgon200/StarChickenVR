#pragma once
#include "DrillLib.h"
#include "DrillMath.h"
#include "StarChicken_decl.h"
#include "XR_decl.h"
#include "VK_decl.h"
namespace XR {

const char* XR_ENABLED_EXTENSIONS[]{ 
#if XR_DEBUG != 0
	XR_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
	"XR_KHR_vulkan_enable2" };

PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr;
PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties;
PFN_xrEnumerateApiLayerProperties xrEnumerateApiLayerProperties;
PFN_xrCreateInstance xrCreateInstance;
#define OP(func) PFN_##func func;
XR_FUNCTIONS
#undef OP

XrInstance xrInstance;
XrDebugUtilsMessengerEXT messenger;
XrSystemId systemID;
XrSession session;
XrSessionState currentSessionState;
XrSwapchain xrSwapchain;

XrSpace stageSpace;
XrSpace localSpace;
XrSpace viewSpace;

u32 xrRenderWidth;
u32 xrRenderHeight;
XrPosef lastValidLeftEyePose;
XrFovf lastValidLeftFov;
XrPosef lastValidRightEyePose;
XrFovf lastValidRightFov;

Quaternionf xr_quat_to_drillmath_quat(XrQuaternionf q) {
	return Quaternionf{ q.x, q.y, q.z, q.w };
}

Vector3f xr_vec3_to_drillmath_vec3(XrVector3f v) {
	return Vector3f{ v.x, v.y, v.z };
}

void openxr_failure(XrResult failureResult) {
	print("XR function failed!\n");
	__debugbreak();
	ExitProcess(EXIT_FAILURE);
}

bool load_first_openxr_functions() {
	HMODULE openxr = LoadLibraryA("openxr_loader.dll");
	if (!openxr) {
		openxr = LoadLibraryA("openxr-loader.dll");
	}
	if (!openxr) {
		openxr = LoadLibraryA("libopenxr_loader.dll");
	}
	if (!openxr) {
		return false;
	}

	xrGetInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(reinterpret_cast<void*>(GetProcAddress(openxr, "xrGetInstanceProcAddr")));
	xrInstance = XR_NULL_HANDLE;
	CHK_XR(xrGetInstanceProcAddr(xrInstance, "xrEnumerateApiLayerProperties", reinterpret_cast<PFN_xrVoidFunction*>(&xrEnumerateApiLayerProperties)));
	CHK_XR(xrGetInstanceProcAddr(xrInstance, "xrEnumerateInstanceExtensionProperties", reinterpret_cast<PFN_xrVoidFunction*>(&xrEnumerateInstanceExtensionProperties)));
	CHK_XR(xrGetInstanceProcAddr(xrInstance, "xrCreateInstance", reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateInstance)));
	
	return true;
}

// Called after instance is created
void load_all_openxr_functions() {
#define OP(func) CHK_XR(xrGetInstanceProcAddr(xrInstance, #func, reinterpret_cast<PFN_xrVoidFunction*>(&func)))
	XR_FUNCTIONS
#undef OP
}

static XRAPI_ATTR XrBool32 XRAPI_CALL debug_callback(
		XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
		XrDebugUtilsMessageTypeFlagsEXT messageType,
		const XrDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		if (messageSeverity > XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			print("XR validation layer: ");
			print(pCallbackData->message);
			print("\n");
		}

		return XR_FALSE;
	}

void init_openxr() {
	if (!load_first_openxr_functions()) {
		print("Failed to load OpenXR Functions\n");
		__debugbreak();
		ExitProcess(EXIT_FAILURE);
	}

	u32 propertyCount{};
	CHK_XR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &propertyCount, nullptr));
	XrExtensionProperties* properties = stackArena.alloc<XrExtensionProperties>(propertyCount);
	zero_memory(properties, propertyCount * sizeof(XrExtensionProperties));
	for (u32 i = 0; i < propertyCount; i++) {
		properties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
	}
	CHK_XR(xrEnumerateInstanceExtensionProperties(nullptr, propertyCount, &propertyCount, properties));
	for (u32 i = 0; i < propertyCount; i++) {
		properties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
		if (strcmp(properties[i].extensionName, "XR_KHR_vulkan_enable2") == 0) {
			goto vulkanSupported;
		}
	}
	print("XR_KHR_vulkan_enable2 not supported, exiting.\n");
	return;
vulkanSupported:;

	XrApplicationInfo applicationInfo{};
	strcpy(applicationInfo.applicationName, "Star Chicken VR");
	applicationInfo.applicationVersion = 1;
	strcpy(applicationInfo.engineName, "DrillEngine");
	applicationInfo.engineVersion = 3;
	applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
	XrInstanceCreateInfo instanceCreateInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
	instanceCreateInfo.applicationInfo = applicationInfo;
	instanceCreateInfo.enabledApiLayerCount = 0;
	instanceCreateInfo.enabledApiLayerNames = nullptr;
	instanceCreateInfo.enabledExtensionCount = ARRAY_COUNT(XR_ENABLED_EXTENSIONS);
	instanceCreateInfo.enabledExtensionNames = XR_ENABLED_EXTENSIONS;
#if XR_DEBUG != 0
	XrDebugUtilsMessengerCreateInfoEXT messengerCreateInfo{ XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	messengerCreateInfo.messageSeverities =
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messengerCreateInfo.messageTypes =
		XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
	messengerCreateInfo.userCallback = debug_callback;
	instanceCreateInfo.next = &messengerCreateInfo;
#endif
	CHK_XR(xrCreateInstance(&instanceCreateInfo, &xrInstance));

	load_all_openxr_functions();

#if XR_DEBUG != 0
	CHK_XR(xrCreateDebugUtilsMessengerEXT(xrInstance, &messengerCreateInfo, &messenger));
#endif

	XrSystemGetInfo systemGetInfo{ XR_TYPE_SYSTEM_GET_INFO };
	systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrResult systemResult = xrGetSystem(xrInstance, &systemGetInfo, &systemID);
	if (systemResult == XR_ERROR_FORM_FACTOR_UNSUPPORTED) {
		print("Does not support HMD\n");
	} else if (systemResult == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
		print("HMD currently unavailable\n");
	}
	CHK_XR(systemResult);
	print("Headset found, initializing graphics...\n");

	XrSystemProperties systemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
	CHK_XR(xrGetSystemProperties(xrInstance, systemID, &systemProperties));
	print("System type is ");
	print(systemProperties.systemName);
	print("\n");

	u32 numViews = 0;
	CHK_XR(xrEnumerateViewConfigurationViews(xrInstance, systemID, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &numViews, nullptr));
	XrViewConfigurationView* views = stackArena.alloc<XrViewConfigurationView>(numViews);
	zero_memory(views, numViews * sizeof(XrViewConfigurationView));
	for (u32 i = 0; i < numViews; i++) {
		views[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
	}
	CHK_XR(xrEnumerateViewConfigurationViews(xrInstance, systemID, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, numViews, &numViews, views));
	if (numViews != 2) {
		abort("OpenXR returned non stereo view count for stereo view configuration");
	}
	xrRenderWidth = views[0].recommendedImageRectWidth;
	xrRenderHeight = views[0].recommendedImageRectHeight;
	lastValidRightEyePose = IDENTITY_POSE;
	lastValidLeftEyePose = IDENTITY_POSE;
	lastValidLeftFov.angleLeft = TURN_TO_RAD(-0.125F);
	lastValidLeftFov.angleRight = TURN_TO_RAD(0.125F);
	lastValidLeftFov.angleUp = TURN_TO_RAD(0.125F);
	lastValidLeftFov.angleDown = TURN_TO_RAD(-0.125F);
	lastValidRightFov = lastValidLeftFov;
}

void create_session_and_vk_swapchain() {
	XrGraphicsBindingVulkan2KHR vulkanBinding{ XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR };
	vulkanBinding.instance = VK::vkInstance;
	vulkanBinding.physicalDevice = VK::physicalDevice;
	vulkanBinding.device = VK::logicalDevice;
	vulkanBinding.queueFamilyIndex = VK::graphicsFamily;
	vulkanBinding.queueIndex = 0;
	XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
	sessionCreateInfo.createFlags = 0;
	sessionCreateInfo.systemId = systemID;
	sessionCreateInfo.next = &vulkanBinding;
	CHK_XR(xrCreateSession(xrInstance, &sessionCreateInfo, &session));

	u32 numReferenceSpaces = 0;
	CHK_XR(xrEnumerateReferenceSpaces(session, 0, &numReferenceSpaces, nullptr));
	XrReferenceSpaceType* referenceSpaceTypes = stackArena.alloc<XrReferenceSpaceType>(numReferenceSpaces);
	zero_memory(referenceSpaceTypes, numReferenceSpaces * sizeof(XrReferenceSpaceType));
	CHK_XR(xrEnumerateReferenceSpaces(session, numReferenceSpaces, &numReferenceSpaces, referenceSpaceTypes));
	for (u32 i = 0; i < numReferenceSpaces; i++) {
		XrReferenceSpaceType type = referenceSpaceTypes[i];
		XrReferenceSpaceCreateInfo referenceSpaceInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
		referenceSpaceInfo.referenceSpaceType = type;
		referenceSpaceInfo.poseInReferenceSpace = IDENTITY_POSE;
		if (type == XR_REFERENCE_SPACE_TYPE_STAGE) {
			CHK_XR(xrCreateReferenceSpace(session, &referenceSpaceInfo, &stageSpace));
		} else if (type == XR_REFERENCE_SPACE_TYPE_LOCAL) {
			CHK_XR(xrCreateReferenceSpace(session, &referenceSpaceInfo, &localSpace));
		} else if (type == XR_REFERENCE_SPACE_TYPE_VIEW) {
			CHK_XR(xrCreateReferenceSpace(session, &referenceSpaceInfo, &viewSpace));
		}
	}
	if (!stageSpace) {
		abort("System did not support stage space\n");
	}
	
	u32 formatCount = 0;
	CHK_XR(xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr));
	i64* supportedSwapchainFormats = stackArena.alloc<i64>(formatCount);
	CHK_XR(xrEnumerateSwapchainFormats(session, formatCount, &formatCount, supportedSwapchainFormats));
	for (u32 i = 0; i < formatCount; i++) {
		if (supportedSwapchainFormats[i] == OPENXR_SWAPCHAIN_IMAGE_FORMAT) {
			goto swapchainFormatSupported;
		}
	}
	abort("Desired swapchain image format not supported!");
swapchainFormatSupported:;

	XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
	swapchainCreateInfo.createFlags = 0;
	swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.format = OPENXR_SWAPCHAIN_IMAGE_FORMAT;
	swapchainCreateInfo.sampleCount = 1;
	swapchainCreateInfo.width = xrRenderWidth;
	swapchainCreateInfo.height = xrRenderHeight;
	swapchainCreateInfo.faceCount = 1;
	swapchainCreateInfo.arraySize = 2; // 2 eyes
	swapchainCreateInfo.mipCount = 1;
	CHK_XR(xrCreateSwapchain(session, &swapchainCreateInfo, &xrSwapchain));

	u32 numSwapchainImages = 0;
	CHK_XR(xrEnumerateSwapchainImages(xrSwapchain, 0, &numSwapchainImages, nullptr));
	XrSwapchainImageVulkan2KHR* swapchainImages = stackArena.alloc<XrSwapchainImageVulkan2KHR>(numSwapchainImages);
	zero_memory(swapchainImages, numSwapchainImages * sizeof(XrSwapchainImageVulkan2KHR));
	for (u32 i = 0; i < numSwapchainImages; i++) {
		swapchainImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR;
	}
	CHK_XR(xrEnumerateSwapchainImages(xrSwapchain, numSwapchainImages, &numSwapchainImages, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages)));
	VK::xrSwapchainData.swapchainImages = globalArena.alloc<VkImage>(numSwapchainImages);
	for (u32 i = 0; i < numSwapchainImages; i++) {
		VK::xrSwapchainData.swapchainImages[i] = swapchainImages[i].image;
	}
	VK::xrSwapchainData.swapchainImageCount = numSwapchainImages;
}

void poll() {
	while (true) {
		XrEventDataBuffer evt{ XR_TYPE_EVENT_DATA_BUFFER };
		XrResult eventResult = xrPollEvent(xrInstance, &evt);
		CHK_XR(eventResult);
		if (eventResult == XR_EVENT_UNAVAILABLE) {
			break;
		}
		switch (evt.type) {
			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
				XrEventDataSessionStateChanged& sessionStateChange = *reinterpret_cast<XrEventDataSessionStateChanged*>(&evt);
				switch (sessionStateChange.state) {
					case XR_SESSION_STATE_IDLE: {
						print("Session idle\n");
					} break;
					case XR_SESSION_STATE_READY: {
						print("Session ready\n");
						XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
						sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
						CHK_XR(xrBeginSession(session, &sessionBeginInfo));
					} break;
					case XR_SESSION_STATE_SYNCHRONIZED: {
						print("Session synchronized\n");
					} break;
					case XR_SESSION_STATE_VISIBLE: {
						print("Session visible\n");
					} break;
					case XR_SESSION_STATE_FOCUSED: {
						print("Session focused\n");
					} break;
					case XR_SESSION_STATE_STOPPING: {
						print("Session stopping\n");
						CHK_XR(xrEndSession(session));
					} break;
					case XR_SESSION_STATE_LOSS_PENDING: {
						print("Session loss pending\n");
						StarChicken::shouldShutDown = true;
					} break;
					case XR_SESSION_STATE_EXITING: {
						print("Session exiting\n");
						StarChicken::shouldShutDown = true;
					} break;
					default: break;
				}
				currentSessionState = sessionStateChange.state;
			} break;
			default: break;
		}
	}
}

NODISCARD OpenXRFrameInfo begin_frame() {
	XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
	XrFrameState frameState{ XR_TYPE_FRAME_STATE };
	CHK_XR(xrWaitFrame(session, &frameWaitInfo, &frameState));

	XrSwapchainImageAcquireInfo swapchainAcquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
	u32 swapchainImageIdx = U32_MAX;
	CHK_XR(xrAcquireSwapchainImage(xrSwapchain, &swapchainAcquireInfo, &swapchainImageIdx));
	VK::xrSwapchainData.swapchainImageIdx = swapchainImageIdx;

	XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
	CHK_XR(xrBeginFrame(session, &frameBeginInfo));

	XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
	viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	viewLocateInfo.displayTime = frameState.predictedDisplayTime;
	viewLocateInfo.space = stageSpace;
	XrViewState viewState{ XR_TYPE_VIEW_STATE };
	XrView views[2]{ { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };
	u32 viewCount = 0;
	CHK_XR(xrLocateViews(session, &viewLocateInfo, &viewState, ARRAY_COUNT(views), &viewCount, views));
	if (viewCount == 2) {
		if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) {
			lastValidLeftEyePose.position = views[0].pose.position;
			lastValidRightEyePose.position = views[1].pose.position;
		}
		if (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
			lastValidLeftEyePose.orientation = views[0].pose.orientation;
			lastValidRightEyePose.orientation = views[1].pose.orientation;
		}
		// Don't support each eye with their own FOV
		lastValidLeftFov = views[0].fov;
		lastValidRightFov = views[1].fov;
	}


	return OpenXRFrameInfo{ lastValidLeftEyePose, lastValidLeftFov, lastValidRightEyePose, lastValidRightFov, frameState.predictedDisplayTime, frameState.shouldRender };
}

void end_frame(OpenXRFrameInfo& frameInfo) {
	XrSwapchainImageReleaseInfo swapchainReleaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
	CHK_XR(xrReleaseSwapchainImage(xrSwapchain, &swapchainReleaseInfo));

	XrCompositionLayerProjectionView views[2]{};
	views[0].type = views[1].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
	views[0].pose = frameInfo.leftEyePose;
	views[1].pose = frameInfo.rightEyePose;
	views[0].fov = frameInfo.leftEyeFov;
	views[1].fov = frameInfo.rightEyeFov;
	views[0].subImage.swapchain = views[1].subImage.swapchain = xrSwapchain;
	views[0].subImage.imageRect = views[1].subImage.imageRect = XrRect2Di{ XrOffset2Di{ 0, 0 }, XrExtent2Di{ i32(xrRenderWidth), i32(xrRenderHeight) } };
	views[0].subImage.imageArrayIndex = 0;
	views[1].subImage.imageArrayIndex = 1;

	XrCompositionLayerProjection displayImages{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	displayImages.layerFlags = 0;
	displayImages.space = stageSpace;
	displayImages.viewCount = ARRAY_COUNT(views);
	displayImages.views = views;
	XrCompositionLayerBaseHeader* compositionLayers[]{ reinterpret_cast<XrCompositionLayerBaseHeader*>(&displayImages) };

	XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
	frameEndInfo.displayTime = frameInfo.predictedDisplayTime;
	frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	frameEndInfo.layerCount = ARRAY_COUNT(compositionLayers);
	frameEndInfo.layers = compositionLayers;
	CHK_XR(xrEndFrame(session, &frameEndInfo));
}

void end_openxr() {
	if (xrSwapchain) {
		xrDestroySwapchain(xrSwapchain);
	}
#if XR_DEBUG != 0
	CHK_XR(xrDestroyDebugUtilsMessengerEXT(messenger));
#endif
	xrDestroyInstance(xrInstance);
}

}