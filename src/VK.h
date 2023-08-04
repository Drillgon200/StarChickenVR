#pragma once
#include "DrillLib.h"
#include "StarChicken_decl.h"
#include "VK_decl.h"
#include "XR_decl.h"
#include "VKStaging.h"
#include "VKGeometry.h"
namespace VK {

#define VK_ENABLE_VALIDATION_LAYERS 1

const char* ENABLED_VALIDATION_LAYERS[]{ "VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2" };

const char* INSTANCE_EXTENSIONS[
#if VK_DEBUG == 0
0
#endif
]{
#if VK_DEBUG != 0	
VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
};

const char* DEVICE_EXTENSIONS[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
PFN_vkCreateInstance vkCreateInstance;
#define OP(name) PFN_##name name;
VK_INSTANCE_FUNCTIONS
VK_DEVICE_FUNCTIONS
#undef OP

VkInstance vkInstance;
VkDebugUtilsMessengerEXT messenger;
VkPhysicalDevice physicalDevice;
VkDevice logicalDevice;

VkPhysicalDeviceProperties physicalDeviceProperties;

u32 graphicsFamily;
u32 transferFamily;
u32 computeFamily;
#define FAMILY_PRESENT_BIT_GRAPHICS (1 << 0)
#define FAMILY_PRESENT_BIT_TRANSFER (1 << 1)
#define FAMILY_PRESENT_BIT_COMPUTE (1 << 2)
u32 familyPresentBits;

u32 hostMemoryTypeIndex;
u32 deviceMemoryTypeIndex;

VkQueue graphicsQueue;
VkQueue transferQueue;
VkQueue computeQueue;

VKStaging::GPUUploadStager graphicsStager;
VKGeometry::GeometryHandler geometryHandler;

Framebuffer mainFramebuffer;

VkCommandPool graphicsCommandPool;
VkCommandBuffer graphicsCommandBuffer;

VkRenderPass mainRenderPass;

VkPipelineLayout testPipelineLayout;
VkPipeline testPipeline;

VKGeometry::Mesh testMesh;

SwapchainData xrSwapchainData;

void vulkan_failure(const char* msg) {
	print("VK function failed!\n");
	print(msg);
	println();

	__debugbreak();
	ExitProcess(EXIT_FAILURE);
}

b32 load_first_vulkan_functions() {
	HMODULE vulkan = LoadLibraryA("vulkan-1.dll");
	if (!vulkan) {
		print("vulkan-1.dll not found! Perhaps upgrade your graphics drivers?\n");
		return false;
	}
	if ((vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vulkan, "vkGetInstanceProcAddr"))) == nullptr) {
		return false;
	}
	if ((vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"))) == nullptr) {
		return false;
	}
	return true;
}

void load_instance_vulkan_functions() {
#define OP(name) CHK_VK_NOT_NULL(name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(vkInstance, #name)), #name);
	VK_INSTANCE_FUNCTIONS
#undef OP
}

void load_device_vulkan_functions() {
#define OP(name) CHK_VK_NOT_NULL(name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(logicalDevice, #name)), #name);
	VK_DEVICE_FUNCTIONS
#undef OP
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		print("VK validation layer: ");
		print(pCallbackData->pMessage);
		print("\n");
	}

	// According to spec, I should always return VK_FALSE here, VK_TRUE is reserved for layers
	return VK_FALSE;
}

void init_vulkan() {
	if (!load_first_vulkan_functions()) {
		print("Failed to load Vulkan Functions\n");
		__debugbreak();
		ExitProcess(EXIT_FAILURE);
	}
	XrGraphicsRequirementsVulkan2KHR xrVulkanRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR };
	CHK_XR(XR::xrGetVulkanGraphicsRequirements2KHR(XR::xrInstance, XR::systemID, &xrVulkanRequirements));
	if (XR_MAKE_VERSION(1, 2, 0) < xrVulkanRequirements.minApiVersionSupported) {
		print("OpenXR minimum vulkan version is greater than our vulkan version, exiting");
		__debugbreak();
		ExitProcess(EXIT_FAILURE);
	}

	/*u32 instanceExtensionCount{};
	CHK_XR(XR::xrGetVulkanInstanceExtensionsKHR(XR::xrInstance, XR::systemID, 0, &instanceExtensionCount, nullptr));
	char* xrRequiredExtensions = stackArena.alloc<char>(instanceExtensionCount + 1);
	CHK_XR(XR::xrGetVulkanInstanceExtensionsKHR(XR::xrInstance, XR::systemID, instanceExtensionCount, &instanceExtensionCount, xrRequiredExtensions));
	xrRequiredExtensions[instanceExtensionCount] = 0;*/

	ArenaArrayList<const char*> enabledLayers{};
	enabledLayers.push_back_n(ENABLED_VALIDATION_LAYERS, ARRAY_COUNT(ENABLED_VALIDATION_LAYERS));
	ArenaArrayList<const char*> enabledExtensions{};
	enabledExtensions.push_back_n(INSTANCE_EXTENSIONS, ARRAY_COUNT(INSTANCE_EXTENSIONS));
	/*while(*xrRequiredExtensions){
		enabledExtensions.push_back(xrRequiredExtensions);
		while (*xrRequiredExtensions && *xrRequiredExtensions != ' ') {
			xrRequiredExtensions++;
		}
		if (*xrRequiredExtensions == ' ') {
			*xrRequiredExtensions++ = '\0';
		}
	}*/

	VkApplicationInfo applicationInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	applicationInfo.pApplicationName = "Star Chicken VR";
	applicationInfo.applicationVersion = 1;
	applicationInfo.pEngineName = "DrillEngine";
	applicationInfo.engineVersion = 3;
	applicationInfo.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo instanceCreateInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instanceCreateInfo.flags = 0;
	instanceCreateInfo.pApplicationInfo = &applicationInfo;
	instanceCreateInfo.enabledLayerCount = enabledLayers.size;
	instanceCreateInfo.ppEnabledLayerNames = enabledLayers.data;
	instanceCreateInfo.enabledExtensionCount = enabledExtensions.size;
	instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data;
#if VK_DEBUG != 0
	VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	messengerCreateInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messengerCreateInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	messengerCreateInfo.pfnUserCallback = debug_callback;
	instanceCreateInfo.pNext = &messengerCreateInfo;
#endif
	XrVulkanInstanceCreateInfoKHR xrInstanceCreateInfo{ XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR };
	xrInstanceCreateInfo.systemId = XR::systemID;
	xrInstanceCreateInfo.createFlags = 0;
	xrInstanceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
	xrInstanceCreateInfo.vulkanCreateInfo = &instanceCreateInfo;
	xrInstanceCreateInfo.vulkanAllocator = VK_NULL_HANDLE;
	VkResult instanceCreateResult = VK_SUCCESS;
	CHK_XR(XR::xrCreateVulkanInstanceKHR(XR::xrInstance, &xrInstanceCreateInfo, &vkInstance, &instanceCreateResult));
	CHK_VK(instanceCreateResult);

	load_instance_vulkan_functions();

#if VK_DEBUG != 0
	CHK_VK(vkCreateDebugUtilsMessengerEXT(vkInstance, &messengerCreateInfo, VK_NULL_HANDLE, &messenger));
#endif

	XrVulkanGraphicsDeviceGetInfoKHR xrGraphicsDeviceInfo{ XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR };
	xrGraphicsDeviceInfo.systemId = XR::systemID;
	xrGraphicsDeviceInfo.vulkanInstance = vkInstance;
	CHK_XR(XR::xrGetVulkanGraphicsDevice2KHR(XR::xrInstance, &xrGraphicsDeviceInfo, &physicalDevice));

	u32 familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
	VkQueueFamilyProperties* queueFamilyProperties = stackArena.alloc<VkQueueFamilyProperties>(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, queueFamilyProperties);
	for (u32 i = 0; i < familyCount; i++) {
		u32 queueFlags = queueFamilyProperties[i].queueFlags;
		if (queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsFamily = i;
			familyPresentBits |= FAMILY_PRESENT_BIT_GRAPHICS;
		}
		if (queueFlags & VK_QUEUE_TRANSFER_BIT) {
			transferFamily = i;
			familyPresentBits |= FAMILY_PRESENT_BIT_TRANSFER;
		}
		if (queueFlags & VK_QUEUE_COMPUTE_BIT) {
			computeFamily = i;
			familyPresentBits |= FAMILY_PRESENT_BIT_COMPUTE;
		}
		if (familyPresentBits == (FAMILY_PRESENT_BIT_GRAPHICS | FAMILY_PRESENT_BIT_TRANSFER | FAMILY_PRESENT_BIT_COMPUTE)) {
			goto allNecessaryQueuesPresent;
		}
	}
	print("Physical device did not have a graphics, transfer, and compute capable queue!\n");
	__debugbreak();
	ExitProcess(EXIT_FAILURE);
allNecessaryQueuesPresent:;

	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	float queuePriority = 1.0F;
	VkDeviceQueueCreateInfo queueCreateInfos[3]{};
	u32 queueCreateInfoCount = 1;
	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[0].queueFamilyIndex = graphicsFamily;
	queueCreateInfos[0].queueCount = 1;
	queueCreateInfos[0].pQueuePriorities = &queuePriority;
	if (transferFamily != graphicsFamily) {
		queueCreateInfoCount++;
		queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[1].queueFamilyIndex = transferFamily;
		queueCreateInfos[1].queueCount = 1;
		queueCreateInfos[1].pQueuePriorities = &queuePriority;
	}
	if (computeFamily != graphicsFamily && computeFamily != transferFamily) {
		queueCreateInfoCount++;
		queueCreateInfos[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[2].queueFamilyIndex = computeFamily;
		queueCreateInfos[2].queueCount = 1;
		queueCreateInfos[2].pQueuePriorities = &queuePriority;
	}

	VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
	deviceCreateInfo.enabledLayerCount = enabledLayers.size;
	deviceCreateInfo.ppEnabledLayerNames = enabledLayers.data;
	deviceCreateInfo.enabledExtensionCount = ARRAY_COUNT(DEVICE_EXTENSIONS);
	deviceCreateInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS;

	VkPhysicalDeviceFeatures deviceEnabledFeatures{};
	deviceEnabledFeatures.multiDrawIndirect = VK_TRUE;
	deviceEnabledFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
	deviceEnabledFeatures.textureCompressionBC = VK_TRUE;
	deviceEnabledFeatures.fullDrawIndexUint32 = VK_TRUE;
	// I don't actually use this one, but some shader in Oculus's OpenXR implementation seems to, and this gets rid of the validation error
	deviceEnabledFeatures.shaderStorageImageMultisample = VK_TRUE;
	deviceCreateInfo.pEnabledFeatures = &deviceEnabledFeatures;
	VkPhysicalDeviceVulkan11Features deviceEnabledFeatures11{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	deviceEnabledFeatures11.multiview = VK_TRUE;
	deviceCreateInfo.pNext = &deviceEnabledFeatures11;
	VkPhysicalDeviceVulkan12Features deviceEnabledFeatures12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	deviceEnabledFeatures12.descriptorIndexing = VK_TRUE;
	deviceEnabledFeatures12.runtimeDescriptorArray = VK_TRUE;
	deviceEnabledFeatures12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
	deviceEnabledFeatures12.samplerFilterMinmax = VK_TRUE;
	deviceEnabledFeatures12.vulkanMemoryModel = VK_TRUE;
	deviceEnabledFeatures11.pNext = &deviceEnabledFeatures12;

	XrVulkanDeviceCreateInfoKHR xrDeviceCreateInfo{ XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR };
	xrDeviceCreateInfo.systemId = XR::systemID;
	xrDeviceCreateInfo.createFlags = 0;
	xrDeviceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
	xrDeviceCreateInfo.vulkanPhysicalDevice = physicalDevice;
	xrDeviceCreateInfo.vulkanCreateInfo = &deviceCreateInfo;
	VkResult deviceCreationResult = VK_SUCCESS;
	CHK_XR(XR::xrCreateVulkanDeviceKHR(XR::xrInstance, &xrDeviceCreateInfo, &logicalDevice, &deviceCreationResult));
	CHK_VK(deviceCreationResult);

	load_device_vulkan_functions();

	vkGetDeviceQueue(logicalDevice, graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(logicalDevice, transferFamily, 0, &transferQueue);
	vkGetDeviceQueue(logicalDevice, computeFamily, 0, &computeQueue);

	VkCommandPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolCreateInfo.flags = 0;
	poolCreateInfo.queueFamilyIndex = graphicsFamily;
	CHK_VK(vkCreateCommandPool(logicalDevice, &poolCreateInfo, VK_NULL_HANDLE, &graphicsCommandPool));
	VkCommandBufferAllocateInfo cmdBufInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmdBufInfo.commandPool = graphicsCommandPool;
	cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufInfo.commandBufferCount = 1;
	CHK_VK(vkAllocateCommandBuffers(logicalDevice, &cmdBufInfo, &graphicsCommandBuffer));

	hostMemoryTypeIndex = U32_MAX;
	deviceMemoryTypeIndex = U32_MAX;
	u32 hostHeapIdx{};
	u32 deviceHeapIdx{};
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
	for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
		VkMemoryType type = memoryProperties.memoryTypes[i];
		if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT && (hostMemoryTypeIndex == U32_MAX || memoryProperties.memoryHeaps[type.heapIndex].size > memoryProperties.memoryHeaps[hostHeapIdx].size)) {
			hostHeapIdx = type.heapIndex;
			hostMemoryTypeIndex = i;
		}
		if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && (deviceMemoryTypeIndex == U32_MAX || memoryProperties.memoryHeaps[type.heapIndex].size > memoryProperties.memoryHeaps[deviceHeapIdx].size)) {
			deviceHeapIdx = type.heapIndex;
			deviceMemoryTypeIndex = i;
		}
	}

	if (hostMemoryTypeIndex == U32_MAX || deviceMemoryTypeIndex == U32_MAX) {
		abort("Did not have required vulkan memory types\n");
	}
	
	graphicsStager.init(graphicsQueue, graphicsFamily);
	geometryHandler.init(100 * ONE_MEGABYTE);
}


// We're going all in on builders this time. They're a really easy way to provide sensible defaults in a lightweight way

Framebuffer& Framebuffer::set_default() {
	renderPass = VK_NULL_HANDLE;
	framebufferWidth = 0;
	framebufferHeight = 0;
	attachmentCount = 0;
	return *this;
}

Framebuffer& Framebuffer::render_pass(VkRenderPass pass) {
	renderPass = pass;
	return *this;
}

Framebuffer& Framebuffer::dimensions(u32 width, u32 height) {
	framebufferWidth = width;
	framebufferHeight = height;
	return *this;
}

Framebuffer& Framebuffer::new_attachment(VkFormat imageFormat, VkImageUsageFlags usage, VkImageAspectFlags aspectMask, u32 layerCount) {
	if (attachmentCount == MAX_FRAMEBUFFER_ATTACHMENTS) {
		abort("Max framebuffer attachments exceeded");
	}

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.flags = 0;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = imageFormat;
	imageInfo.extent.width = framebufferWidth;
	imageInfo.extent.height = framebufferHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = layerCount;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.queueFamilyIndexCount = 0;
	imageInfo.pQueueFamilyIndices = nullptr;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage image;
	CHK_VK(vkCreateImage(logicalDevice, &imageInfo, nullptr, &image));
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(logicalDevice, image, &memoryRequirements);
	if (!(memoryRequirements.memoryTypeBits & (1 << deviceMemoryTypeIndex))) {
		abort("Could not create framebuffer image in device local memory");
	}
	VkDeviceMemory memory;
	VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocInfo.allocationSize = memoryRequirements.size;
	allocInfo.memoryTypeIndex = deviceMemoryTypeIndex;
	VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	// TODO dedicated allocation
	CHK_VK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &memory));
	CHK_VK(vkBindImageMemory(logicalDevice, image, memory, 0));

	VkImageViewCreateInfo imageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	imageViewInfo.flags = 0;
	imageViewInfo.image = image;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = imageFormat;
	imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewInfo.subresourceRange.aspectMask = aspectMask;
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = layerCount;
	VkImageView view;
	CHK_VK(vkCreateImageView(logicalDevice, &imageViewInfo, nullptr, &view));

	FramebufferAttachment& attachment = attachments[attachmentCount++];
	attachment.memory = memory;
	attachment.image = image;
	attachment.imageView = view;
	attachment.ownsImageView = true;
	return *this;
}

Framebuffer& Framebuffer::existing_attachment(VkImageView view) {
	if (attachmentCount == MAX_FRAMEBUFFER_ATTACHMENTS) {
		abort("Max framebuffer attachments exceeded");
	}

	FramebufferAttachment& attachment = attachments[attachmentCount++];
	attachment.memory = VK_NULL_HANDLE;
	attachment.image = VK_NULL_HANDLE;
	attachment.imageView = view;
	attachment.ownsImageView = false;
	return *this;
}

Framebuffer& Framebuffer::build() {
	if (renderPass == VK_NULL_HANDLE) {
		abort("Render pass must be set to create framebuffer");
	}
	if (framebufferWidth == 0 || framebufferHeight == 0) {
		abort("Framebuffer dimensions must be non-zero");
	}

	VkImageView attachmentViews[MAX_FRAMEBUFFER_ATTACHMENTS];
	for (u32 i = 0; i < MAX_FRAMEBUFFER_ATTACHMENTS; i++) {
		attachmentViews[i] = attachments[i].imageView;
	}
	VkFramebufferCreateInfo framebufferInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	framebufferInfo.flags = 0;
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.attachmentCount = attachmentCount;
	framebufferInfo.pAttachments = attachmentViews;
	framebufferInfo.width = framebufferWidth;
	framebufferInfo.height = framebufferHeight;
	framebufferInfo.layers = 1;
	CHK_VK(vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &framebuffer));
	return *this;;
}

void Framebuffer::destroy() {
	vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
	for (u32 i = 0; i < attachmentCount; i++) {
		FramebufferAttachment& attachment = attachments[i];
		if (attachment.ownsImageView) {
			vkDestroyImageView(logicalDevice, attachment.imageView, nullptr);
		}
		if (attachment.image != VK_NULL_HANDLE) {
			vkDestroyImage(logicalDevice, attachment.image, nullptr);
		}
		if (attachment.memory != VK_NULL_HANDLE) {
			vkFreeMemory(logicalDevice, attachment.memory, nullptr);
		}
	}
}

struct RenderPassBuilder {
	static constexpr u32 MAX_COLOR_ATTACHMENTS = 4;
	static constexpr u32 MAX_DEPTH_ATTACHMENTS = 1;
	static constexpr u32 MAX_ATTACHMENTS = MAX_COLOR_ATTACHMENTS + MAX_DEPTH_ATTACHMENTS;
	VkAttachmentDescription attachmentDescriptions[MAX_ATTACHMENTS];
	u32 numAttachments;
	b32 hasDepthAttachment;

	RenderPassBuilder& set_default() {
		numAttachments = 0;
		hasDepthAttachment = false;
		return *this;
	}

	RenderPassBuilder& color_attachment(VkFormat format) {
		if (hasDepthAttachment) {
			abort("Color attachments must be defined before depth attachment");
		}
		if (numAttachments == MAX_COLOR_ATTACHMENTS) {
			abort("Max attachment count exceeded");
		}
		VkAttachmentDescription& attachmentDesc = attachmentDescriptions[numAttachments++];
		attachmentDesc.flags = 0;
		attachmentDesc.format = format;
		attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		return *this;
	}

	RenderPassBuilder& depth_attachment(VkFormat format) {
		if (hasDepthAttachment) {
			abort("Color attachments must be defined before depth attachment");
		}
		if (numAttachments == MAX_COLOR_ATTACHMENTS) {
			abort("Max attachment count exceeded");
		}
		VkAttachmentDescription& attachmentDesc = attachmentDescriptions[numAttachments++];
		attachmentDesc.flags = 0;
		attachmentDesc.format = format;
		attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		hasDepthAttachment = true;
		return *this;
	}

	VkRenderPass build() {
		VkAttachmentReference colorRefs[MAX_COLOR_ATTACHMENTS];
		for (u32 i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
			colorRefs[i].attachment = i;
			colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		VkAttachmentReference depthRef;
		depthRef.attachment = numAttachments - 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.flags = 0;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = nullptr;
		subpass.colorAttachmentCount = numAttachments - hasDepthAttachment;
		subpass.pColorAttachments = colorRefs;
		subpass.pResolveAttachments = nullptr;
		if (hasDepthAttachment) {
			subpass.pDepthStencilAttachment = &depthRef;
		} else {
			subpass.pDepthStencilAttachment = nullptr;
		}
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = nullptr;
		VkRenderPassCreateInfo passInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		passInfo.flags = 0;
		passInfo.attachmentCount = numAttachments;
		passInfo.pAttachments = attachmentDescriptions;
		passInfo.subpassCount = 1;
		passInfo.pSubpasses = &subpass;
		passInfo.dependencyCount = 0;
		passInfo.pDependencies = nullptr;
		VkRenderPassMultiviewCreateInfo renderPassMultiview{ VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
		renderPassMultiview.subpassCount = 1;
		u32 viewMask = 0b11; // broadcast to both eyes
		renderPassMultiview.pViewMasks = &viewMask;
		renderPassMultiview.dependencyCount = 0;
		renderPassMultiview.pViewOffsets = 0;
		u32 correlationMask = 0b11; // the two eyes will be very similar
		renderPassMultiview.correlationMaskCount = 1;
		renderPassMultiview.pCorrelationMasks = &correlationMask;
		passInfo.pNext = &renderPassMultiview;
		VkRenderPass renderPass;
		CHK_VK(vkCreateRenderPass(logicalDevice, &passInfo, nullptr, &renderPass));
		return renderPass;
	}
};

void create_render_targets() {
	xrSwapchainData.swapchainImageViews = globalArena.alloc<VkImageView>(xrSwapchainData.swapchainImageCount);
	for (u32 i = 0; i < xrSwapchainData.swapchainImageCount; i++) {
		VkImageViewCreateInfo imageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		imageViewInfo.flags = 0;
		imageViewInfo.image = xrSwapchainData.swapchainImages[i];
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = OPENXR_SWAPCHAIN_IMAGE_FORMAT;
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 2; // 2 eyes
		CHK_VK(vkCreateImageView(logicalDevice, &imageViewInfo, VK_NULL_HANDLE, &xrSwapchainData.swapchainImageViews[i]));
	}

	mainRenderPass = RenderPassBuilder{}.set_default()
		.color_attachment(VK_FORMAT_R8G8B8A8_SRGB)
		.depth_attachment(VK_FORMAT_D32_SFLOAT)
		.build();
	mainFramebuffer.set_default()
		.render_pass(mainRenderPass)
		.dimensions(XR::xrRenderWidth, XR::xrRenderHeight)
		.new_attachment(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 2)
		.new_attachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, 2)
		.build();
}

struct PipelineLayoutBuilder {
	static constexpr u32 pushConstantCap = 1;
	VkPushConstantRange pushConstants[pushConstantCap];
	u32 pushConstantCount;

	PipelineLayoutBuilder& set_default() {
		pushConstantCount = 0;
		return *this;
	}

	PipelineLayoutBuilder& push_constant(VkShaderStageFlags stage, u32 offset, u32 size) {
		if (pushConstantCount == pushConstantCap) {
			// Since we create all pipeline layouts at startup, this will always trigger in testing, so it's almost like a static assert
			abort("PipelineLayoutInfo push constant capacity exceeded");
		}
		pushConstants[pushConstantCount++] = VkPushConstantRange{ stage, offset, size };
		return *this;
	}

	VkPipelineLayout build() {
		VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		layoutInfo.flags = 0;
		layoutInfo.setLayoutCount = 0;
		layoutInfo.pSetLayouts = nullptr;
		layoutInfo.pushConstantRangeCount = pushConstantCount;
		layoutInfo.pPushConstantRanges = pushConstants;
		VkPipelineLayout layout;
		CHK_VK(vkCreatePipelineLayout(logicalDevice, &layoutInfo, VK_NULL_HANDLE, &layout));
		return layout;
	}
};

struct DescriptorSetLayoutBuilder {

	DescriptorSetLayoutBuilder& set_default() {
		return *this;
	}

	VkDescriptorSetLayout build() {
		VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		VkDescriptorSetLayout layout{};
		CHK_VK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, VK_NULL_HANDLE, &layout));
		return layout;
	}
};

struct GraphicsPipelineBuilder {
	static constexpr u32 MAX_ATTRIBUTE_DESCRIPTIONS = 8;
	static constexpr u32 MAX_BINDING_DESCRIPTIONS = 8;

	const char* shaderFileName;
	VkPipelineLayout pipelineLayout;
	VkVertexInputAttributeDescription attributeDescriptions[MAX_ATTRIBUTE_DESCRIPTIONS];
	VkVertexInputBindingDescription bindingDescriptions[MAX_BINDING_DESCRIPTIONS];
	u32 attributeDescriptionCount;
	u32 bindingDescriptionCount;

	GraphicsPipelineBuilder& set_default() {
		shaderFileName = nullptr;
		pipelineLayout = VK_NULL_HANDLE;
		attributeDescriptionCount = 0;
		bindingDescriptionCount= 0;
		return *this;
	}

	GraphicsPipelineBuilder& shader_name(const char* shaderFile) {
		shaderFileName = shaderFile;
		return *this;
	}

	GraphicsPipelineBuilder& layout(VkPipelineLayout layout) {
		pipelineLayout = layout;
		return *this;
	}

	GraphicsPipelineBuilder& vertex_attribute(u32 binding, u32 location, VkFormat format, u32 size) {
		if (attributeDescriptionCount == MAX_ATTRIBUTE_DESCRIPTIONS) {
			abort("Maximum attribute descriptions exceeded");
		}
		if (binding != bindingDescriptionCount && binding != bindingDescriptionCount - 1) {
			abort("Binding must be either the same as current or the next binding");
		}
		if (binding == MAX_BINDING_DESCRIPTIONS) {
			abort("Maximum binding descriptions exceeded");
		}
		VkVertexInputAttributeDescription& attr = attributeDescriptions[attributeDescriptionCount++];
		VkVertexInputBindingDescription& bind = bindingDescriptions[binding];
		if (binding == bindingDescriptionCount) {
			bind.binding = binding;
			bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			bind.stride = 0;
			bindingDescriptionCount++;
		}
		attr.location = location;
		attr.binding = binding;
		attr.format = format;
		attr.offset = bind.stride;
		bind.stride += size;
		return *this;
	}

	VkPipeline build() {
		if (pipelineLayout == VK_NULL_HANDLE) {
			abort("Must set pipeline layout to build VkPipeline");
		}
		if (shaderFileName == nullptr) {
			abort("Must set shader file name to build VkPipeline");
		}

		stackArena.push_frame();
		u32* spirv;
		u32 spirvDwordCount;
		if (!(spirv = read_full_file_to_arena<u32>(&spirvDwordCount, stackArena, shaderFileName))) {
			print("File read failed: ");
			println(shaderFileName);
			abort("Failed reading spv file");
		}
		VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		moduleInfo.flags = 0;
		moduleInfo.codeSize = spirvDwordCount * sizeof(u32);
		moduleInfo.pCode = spirv;
		VkShaderModule shaderModule;
		CHK_VK(vkCreateShaderModule(logicalDevice, &moduleInfo, VK_NULL_HANDLE, &shaderModule));
		stackArena.pop_frame();

		VkPipelineShaderStageCreateInfo stageInfos[]{ { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO }, { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
		VkPipelineShaderStageCreateInfo& vertexStageInfo = stageInfos[0];
		vertexStageInfo.flags = 0;
		vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertexStageInfo.module = shaderModule;
		vertexStageInfo.pName = "vert_main";
		vertexStageInfo.pSpecializationInfo = nullptr;
		VkPipelineShaderStageCreateInfo& fragmentStageInfo = stageInfos[1];
		fragmentStageInfo.flags = 0;
		fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragmentStageInfo.module = shaderModule;
		fragmentStageInfo.pName = "frag_main";
		fragmentStageInfo.pSpecializationInfo = nullptr;

		VkPipelineVertexInputStateCreateInfo vertexInputStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vertexInputStateInfo.flags = 0;
		vertexInputStateInfo.vertexBindingDescriptionCount = bindingDescriptionCount;
		vertexInputStateInfo.pVertexBindingDescriptions = bindingDescriptions;
		vertexInputStateInfo.vertexAttributeDescriptionCount = attributeDescriptionCount;
		vertexInputStateInfo.pVertexAttributeDescriptions = attributeDescriptions;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		inputAssemblyStateInfo.flags = 0;
		inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

		// Viewport and scissor don't matter here since we set them as dynamic state
		VkViewport viewport{};
		viewport.x = 0.0F;
		viewport.y = 0.0F;
		viewport.width = XR::xrRenderWidth;
		viewport.height = XR::xrRenderHeight;
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;
		VkRect2D scissor{};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = XR::xrRenderWidth;
		scissor.extent.height = XR::xrRenderHeight;
		VkPipelineViewportStateCreateInfo inputViewportStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		inputViewportStateInfo.flags = 0;
		inputViewportStateInfo.viewportCount = 1;
		inputViewportStateInfo.pViewports = &viewport;
		inputViewportStateInfo.scissorCount = 1;
		inputViewportStateInfo.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizationStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizationStateInfo.flags = 0;
		rasterizationStateInfo.depthClampEnable = VK_FALSE;
		rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateInfo.cullMode = VK_CULL_MODE_NONE;
		rasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationStateInfo.depthBiasEnable = VK_FALSE;
		rasterizationStateInfo.depthBiasConstantFactor = 0.0F;
		rasterizationStateInfo.depthBiasClamp = 0.0F;
		rasterizationStateInfo.depthBiasSlopeFactor = 0.0F;
		rasterizationStateInfo.lineWidth = 1.0F;

		VkPipelineMultisampleStateCreateInfo multisampleStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisampleStateInfo.flags = 0;
		multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleStateInfo.sampleShadingEnable = VK_FALSE;
		multisampleStateInfo.minSampleShading = 0.0F;
		multisampleStateInfo.pSampleMask = nullptr;
		multisampleStateInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleStateInfo.alphaToOneEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depthStencilStateInfo.flags = 0;
		depthStencilStateInfo.depthTestEnable = VK_TRUE;
		depthStencilStateInfo.depthWriteEnable = VK_TRUE;
		depthStencilStateInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateInfo.stencilTestEnable = VK_FALSE;
		depthStencilStateInfo.front = VkStencilOpState{};
		depthStencilStateInfo.back = VkStencilOpState{};
		depthStencilStateInfo.minDepthBounds = 0.0F;
		depthStencilStateInfo.maxDepthBounds = 1.0F;

		VkPipelineColorBlendAttachmentState mainAttachmentState{};
		mainAttachmentState.blendEnable = VK_FALSE;
		mainAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		mainAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		mainAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		mainAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		mainAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		mainAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		mainAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo colorBlendStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlendStateInfo.flags = 0;
		colorBlendStateInfo.logicOpEnable = VK_FALSE;
		colorBlendStateInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendStateInfo.attachmentCount = 1;
		colorBlendStateInfo.pAttachments = &mainAttachmentState;
		colorBlendStateInfo.blendConstants[0] = 0.0F;
		colorBlendStateInfo.blendConstants[1] = 0.0F;
		colorBlendStateInfo.blendConstants[2] = 0.0F;
		colorBlendStateInfo.blendConstants[3] = 0.0F;

		VkDynamicState dynamicStates[]{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicStateInfo.flags = 0;
		dynamicStateInfo.dynamicStateCount = ARRAY_COUNT(dynamicStates);
		dynamicStateInfo.pDynamicStates = dynamicStates;

		VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineInfo.flags = 0;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stageInfos;
		pipelineInfo.pVertexInputState = &vertexInputStateInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pViewportState = &inputViewportStateInfo;
		pipelineInfo.pRasterizationState = &rasterizationStateInfo;
		pipelineInfo.pMultisampleState = &multisampleStateInfo;
		pipelineInfo.pDepthStencilState = &depthStencilStateInfo;
		pipelineInfo.pColorBlendState = &colorBlendStateInfo;
		pipelineInfo.pDynamicState = &dynamicStateInfo;
		pipelineInfo.layout = testPipelineLayout;
		pipelineInfo.renderPass = mainRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = 0;
		VkPipeline pipeline;
		CHK_VK(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

		vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);

		return pipeline;
	}
};

struct ComputePipelineBuilder {
	VkPipelineLayout pipelineLayout;
	const char* shaderFileName;

	ComputePipelineBuilder& set_default() {
		pipelineLayout = VK_NULL_HANDLE;
		shaderFileName = nullptr;
		return *this;
	}

	ComputePipelineBuilder& shader_name(const char* fileName) {
		shaderFileName = fileName;
		return *this;
	}

	ComputePipelineBuilder& layout(VkPipelineLayout layout) {
		pipelineLayout = layout;
		return *this;
	}

	VkPipeline build() {
		if (pipelineLayout == VK_NULL_HANDLE) {
			abort("Must set pipeline layout to build VkPipeline");
		}
		if (shaderFileName == nullptr) {
			abort("Must set shader file name to build VkPipeline");
		}

		stackArena.push_frame();
		u32* spirv;
		u32 spirvDwordCount;
		if (!(spirv = read_full_file_to_arena<u32>(&spirvDwordCount, stackArena, shaderFileName))) {
			print("File read failed: ");
			println(shaderFileName);
			abort("Failed reading spv file");
		}
		VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		moduleInfo.flags = 0;
		moduleInfo.codeSize = spirvDwordCount * sizeof(u32);
		moduleInfo.pCode = spirv;
		VkShaderModule shaderModule;
		CHK_VK(vkCreateShaderModule(logicalDevice, &moduleInfo, nullptr, &shaderModule));
		stackArena.pop_frame();

		VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pipelineInfo.flags = 0;
		pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineInfo.stage.flags = 0;
		pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineInfo.stage.module = shaderModule;
		pipelineInfo.stage.pName = "compute_main";
		pipelineInfo.stage.pSpecializationInfo = nullptr;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;
		VkPipeline pipeline;
		CHK_VK(vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, &pipeline));

		vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);

		return pipeline;
	}
};

VKGeometry::Mesh load_mesh(const char* modelFileName) {
	stackArena.push_frame();
	u32 modelFileSize;
	byte* modelData;
	if (!(modelData = read_full_file_to_arena<byte>(&modelFileSize, stackArena, modelFileName))) {
		print("Failed to read model file: ");
		println(modelFileName);
		abort("Failed to read model file");
	}
	ByteBuf modelFile{};
	modelFile.wrap(modelData, modelFileSize);
	if (modelFile.read_u32() != bswap32('DUCK')) {
		abort("Model file header did not match DUCK");
	}
	if (modelFile.read_u32() < DRILL_LIB_MAKE_VERSION(3, 0, 0)) {
		abort("Model file out of date");
	}
	String name = modelFile.read_string();
	u32 numVerts = modelFile.read_u16();
	u32 numIndices = modelFile.read_u16();
	u32 vertexDataSize = numVerts * VERTEX_FORMAT_POS3F_TEX2F_NORM3F_TAN3F_SIZE;
	u32 indexDataSize = numIndices * sizeof(u16);
	if (modelFile.failed) {
		print("Model failed read position: ");
		println_integer(modelFile.offset);
		abort("Model format incorrect");
	}
	if (!modelFile.has_data_left(vertexDataSize + indexDataSize)) {
		abort("Model file does not have enough data for all vertices and indices");
	}
	VKGeometry::GeometryAllocation gpuModelData = geometryHandler.alloc(indexDataSize + vertexDataSize, 64);
	graphicsStager.upload_to_buffer(gpuModelData.buffer, gpuModelData.offset, modelFile.bytes + modelFile.offset, vertexDataSize + indexDataSize);
	Vector3f* vertices = reinterpret_cast<Vector3f*>(modelFile.bytes + modelFile.offset);
	u16* indices = reinterpret_cast<u16*>(modelFile.bytes + modelFile.offset + vertexDataSize);
	stackArena.pop_frame();

	VKGeometry::Mesh mesh;
	mesh.gpuGeometry = gpuModelData;
	mesh.positionsOffset = 0;
	mesh.texcoordsOffset = testMesh.positionsOffset + numVerts * sizeof(Vector3f);
	mesh.normalsOffset = testMesh.texcoordsOffset + numVerts * sizeof(Vector2f);
	mesh.tangentsOffset = testMesh.normalsOffset + numVerts * sizeof(Vector3f);
	mesh.indicesOffset = vertexDataSize;
	mesh.indexCount = numIndices;
	return mesh;
}

void load_resources() {
	
	// Pipelines
	testPipelineLayout = PipelineLayoutBuilder{}.set_default()
		.push_constant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantMatrices))
		.build();
	testPipeline = GraphicsPipelineBuilder{}.set_default()
		.layout(testPipelineLayout)
		.shader_name("./resources/shaders/vrtest.spv")
		.vertex_attribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(Vector3f)) // pos
		.vertex_attribute(1, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(Vector2f)) // tex
		.vertex_attribute(2, 2, VK_FORMAT_R32G32B32_SFLOAT, sizeof(Vector3f)) // norm
		.vertex_attribute(3, 3, VK_FORMAT_R32G32B32_SFLOAT, sizeof(Vector3f)) // tan
		.build();

	// Models
	testMesh = load_mesh("./resources/models/test_level.dmf");

	graphicsStager.flush();
	CHK_VK(vkDeviceWaitIdle(logicalDevice));
}

void begin_frame() {
	CHK_VK(vkResetCommandPool(logicalDevice, graphicsCommandPool, 0));

	VkCommandBufferBeginInfo cmdBufInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	cmdBufInfo.flags = 0;
	cmdBufInfo.pInheritanceInfo = nullptr;
	CHK_VK(vkBeginCommandBuffer(graphicsCommandBuffer, &cmdBufInfo));

	VkRenderPassBeginInfo renderPassBegin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderPassBegin.renderPass = mainRenderPass;
	renderPassBegin.framebuffer = mainFramebuffer.framebuffer;
	renderPassBegin.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ mainFramebuffer.framebufferWidth, mainFramebuffer.framebufferHeight } };
	renderPassBegin.clearValueCount = 2;
	VkClearValue clearValues[2]{};
	clearValues[0].color.float32[0] = 0.0F;
	clearValues[0].color.float32[1] = 0.0F;
	clearValues[0].color.float32[2] = 0.0F;
	clearValues[0].color.float32[3] = 0.0F;
	clearValues[1].depthStencil.depth = 0.0F;
	clearValues[1].depthStencil.stencil = 0;
	renderPassBegin.pClearValues = clearValues;
	vkCmdBeginRenderPass(graphicsCommandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

}

void end_frame() {
	vkCmdEndRenderPass(graphicsCommandBuffer);

	VkImageMemoryBarrier imageTransferBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	imageTransferBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageTransferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	imageTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageTransferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	imageTransferBarrier.srcQueueFamilyIndex = graphicsFamily;
	imageTransferBarrier.dstQueueFamilyIndex = graphicsFamily;
	imageTransferBarrier.image = mainFramebuffer.attachments[0].image;
	imageTransferBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageTransferBarrier.subresourceRange.baseMipLevel = 0;
	imageTransferBarrier.subresourceRange.levelCount = 1;
	imageTransferBarrier.subresourceRange.baseArrayLayer = 0;
	imageTransferBarrier.subresourceRange.layerCount = 2;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageTransferBarrier);
	imageTransferBarrier.srcAccessMask = VK_ACCESS_NONE_KHR;
	imageTransferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageTransferBarrier.image = VK::xrSwapchainData.swapchainImages[VK::xrSwapchainData.swapchainImageIdx];
	imageTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageTransferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageTransferBarrier);

	VkImageBlit finalBlit{};
	finalBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	finalBlit.srcSubresource.mipLevel = 0;
	finalBlit.srcSubresource.baseArrayLayer = 0;
	finalBlit.srcSubresource.layerCount = 2;
	finalBlit.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
	finalBlit.srcOffsets[1] = VkOffset3D{ i32(VK::mainFramebuffer.framebufferWidth), i32(VK::mainFramebuffer.framebufferHeight), 1 };
	finalBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	finalBlit.dstSubresource.mipLevel = 0;
	finalBlit.dstSubresource.baseArrayLayer = 0;
	finalBlit.dstSubresource.layerCount = 2;
	finalBlit.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
	finalBlit.dstOffsets[1] = VkOffset3D{ i32(XR::xrRenderWidth), i32(XR::xrRenderHeight), 1 };
	VK::vkCmdBlitImage(graphicsCommandBuffer, VK::mainFramebuffer.attachments[0].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK::xrSwapchainData.swapchainImages[VK::xrSwapchainData.swapchainImageIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &finalBlit, VK_FILTER_NEAREST);

	imageTransferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageTransferBarrier.dstAccessMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	imageTransferBarrier.image = VK::xrSwapchainData.swapchainImages[VK::xrSwapchainData.swapchainImageIdx];
	imageTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageTransferBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageTransferBarrier);

	CHK_VK(vkEndCommandBuffer(graphicsCommandBuffer));

	VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	//VkPipelineStageFlags waitStages[]{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &graphicsCommandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	CHK_VK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
	CHK_VK(vkQueueWaitIdle(graphicsQueue));
}

void end_vulkan() {
	if (testPipeline) {
		vkDestroyPipeline(logicalDevice, testPipeline, VK_NULL_HANDLE);
	}
	if (mainRenderPass) {
		vkDestroyRenderPass(logicalDevice, mainRenderPass, VK_NULL_HANDLE);
	}
	if (logicalDevice) {
		vkDestroyDevice(logicalDevice, VK_NULL_HANDLE);
	}
#if VK_DEBUG != 0
	vkDestroyDebugUtilsMessengerEXT(vkInstance, messenger, VK_NULL_HANDLE);
#endif
	vkDestroyInstance(vkInstance, VK_NULL_HANDLE);
}

}