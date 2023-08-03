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

VkCommandPool graphicsCommandPool;
VkCommandBuffer graphicsCommandBuffer;

VkRenderPass mainRenderPass;

VkPipelineLayout testPipelineLayout;
VkPipeline testPipeline;

SwapchainData xrSwapchainData;

void vulkan_failure(const char* msg) {
	print("VK function failed!\n");
	print(msg);
	println();

	__debugbreak();
	ExitProcess(EXIT_FAILURE);
}

bool load_first_vulkan_functions() {
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

	return VK_FALSE;
}

//extern "C" int _fltused = 0;

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
	u32 hostHeapIdx;
	u32 deviceHeapIdx;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
	for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
		VkMemoryType type = memoryProperties.memoryTypes[i];
		if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT && (hostMemoryTypeIndex == U32_MAX || memoryProperties.memoryHeaps[type.heapIndex].size > memoryProperties.memoryHeaps[hostHeapIdx].size)) {
			hostHeapIdx = type.heapIndex;
			hostMemoryTypeIndex = i;
		}
		if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && (deviceHeapIdx == U32_MAX || memoryProperties.memoryHeaps[type.heapIndex].size > memoryProperties.memoryHeaps[deviceHeapIdx].size)) {
			deviceHeapIdx = type.heapIndex;
			deviceMemoryTypeIndex = i;
		}
	}

	if (hostMemoryTypeIndex == U32_MAX || deviceMemoryTypeIndex == U32_MAX) {
		abort("Did not have required vulkan memory types\n");
	}

	VkMemoryAllocateInfo memoryAllocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memoryAllocateInfo.allocationSize = 1;
	memoryAllocateInfo.memoryTypeIndex = deviceHeapIdx;
	VkDeviceMemory memory;
	//vkCreateBuffer();
	CHK_VK(vkAllocateMemory(logicalDevice, &memoryAllocateInfo, VK_NULL_HANDLE, &memory));
	
	graphicsStager.init(graphicsQueue, graphicsFamily);
	geometryHandler.init(100 * ONE_MEGABYTE);
}

void create_render_targets() {
	xrSwapchainData.swapchainImageViews = globalArena.alloc<VkImageView>(xrSwapchainData.swapchainImageCount);
	for (u32 i = 0; i < xrSwapchainData.swapchainImageCount; i++) {
		VkImageViewCreateInfo imageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		imageViewInfo.flags = 0;
		imageViewInfo.image = xrSwapchainData.swapchainImages[i];
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = OPENXR_SWAPCHAIN_IMAGE_FORMAT;
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 2; // 2 eyes
		CHK_VK(vkCreateImageView(logicalDevice, &imageViewInfo, VK_NULL_HANDLE, &xrSwapchainData.swapchainImageViews[i]));
	}

	VkAttachmentReference swapchainAttachmentRef{};
	swapchainAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	swapchainAttachmentRef.attachment = 0;
	VkSubpassDescription mainRenderPassSubpass{};
	mainRenderPassSubpass.flags = 0;
	mainRenderPassSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	mainRenderPassSubpass.inputAttachmentCount = 0;
	mainRenderPassSubpass.pInputAttachments = nullptr;
	mainRenderPassSubpass.colorAttachmentCount = 1;
	mainRenderPassSubpass.pColorAttachments = &swapchainAttachmentRef;
	mainRenderPassSubpass.pResolveAttachments = nullptr;
	mainRenderPassSubpass.pDepthStencilAttachment = nullptr;
	mainRenderPassSubpass.preserveAttachmentCount = 0;
	mainRenderPassSubpass.pPreserveAttachments = nullptr;

	VkAttachmentDescription swapchainAttachment{};
	swapchainAttachment.flags = 0;
	swapchainAttachment.format = OPENXR_SWAPCHAIN_IMAGE_FORMAT;
	swapchainAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	swapchainAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapchainAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	swapchainAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	renderPassInfo.flags = 0;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &swapchainAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &mainRenderPassSubpass;
	renderPassInfo.dependencyCount = 0;
	renderPassInfo.pDependencies = nullptr;
	VkRenderPassMultiviewCreateInfo renderPassMultiview{ VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
	renderPassMultiview.subpassCount = 1;
	u32 viewMask = 0b11; // broadcast to both eyes
	renderPassMultiview.pViewMasks = &viewMask;
	renderPassMultiview.dependencyCount = 0;
	renderPassMultiview.pViewOffsets = 0;
	u32 correlationMask = 0b11; // the two eyes will be very similar
	renderPassMultiview.correlationMaskCount = 1;
	renderPassMultiview.pCorrelationMasks = &correlationMask;
	renderPassInfo.pNext = &renderPassMultiview;
	CHK_VK(vkCreateRenderPass(logicalDevice, &renderPassInfo, VK_NULL_HANDLE, &mainRenderPass));

	xrSwapchainData.swapchainFramebuffers = globalArena.alloc<VkFramebuffer>(xrSwapchainData.swapchainImageCount);
	for (u32 i = 0; i < xrSwapchainData.swapchainImageCount; i++) {
		VkFramebufferCreateInfo framebufferInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferInfo.flags = 0;
		framebufferInfo.renderPass = mainRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &xrSwapchainData.swapchainImageViews[i];
		framebufferInfo.width = XR::xrRenderWidth;
		framebufferInfo.height = XR::xrRenderHeight;
		framebufferInfo.layers = 1;
		CHK_VK(vkCreateFramebuffer(logicalDevice, &framebufferInfo, VK_NULL_HANDLE, &xrSwapchainData.swapchainFramebuffers[i]));
	}
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

	const char* shaderFileName;
	VkPipelineLayout pipelineLayout;
	VkVertexInputAttributeDescription attributeDescriptions[MAX_ATTRIBUTE_DESCRIPTIONS];
	u32 attributeDescriptionCount;
	u32 currentVertexFormatOffset;

	GraphicsPipelineBuilder& set_default() {
		shaderFileName = nullptr;
		pipelineLayout = VK_NULL_HANDLE;
		attributeDescriptionCount = 0;
		currentVertexFormatOffset = 0;
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

	GraphicsPipelineBuilder& vertex_attribute(u32 location, VkFormat format, u32 size) {
		if (attributeDescriptionCount == MAX_ATTRIBUTE_DESCRIPTIONS) {
			abort("Maximum attribute descriptions exceeded");
		}
		VkVertexInputAttributeDescription& desc = attributeDescriptions[attributeDescriptionCount++];
		desc.location = location;
		desc.binding = 0;
		desc.format = format;
		desc.offset = currentVertexFormatOffset;
		currentVertexFormatOffset += size;
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

		VkVertexInputBindingDescription bindingDescription;
		bindingDescription.binding = 0;
		bindingDescription.stride = currentVertexFormatOffset;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		VkPipelineVertexInputStateCreateInfo vertexInputStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vertexInputStateInfo.flags = 0;
		vertexInputStateInfo.vertexBindingDescriptionCount = 1;
		vertexInputStateInfo.pVertexBindingDescriptions = &bindingDescription;
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
		depthStencilStateInfo.depthTestEnable = VK_FALSE;
		depthStencilStateInfo.depthWriteEnable = VK_FALSE;
		depthStencilStateInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateInfo.stencilTestEnable = VK_FALSE;
		depthStencilStateInfo.front = {};
		depthStencilStateInfo.back = {};
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

void load_resources() {
	testPipelineLayout = PipelineLayoutBuilder{}.set_default().push_constant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantMatrices)).build();
	testPipeline = GraphicsPipelineBuilder{}.set_default().layout(testPipelineLayout).shader_name("./resources/shaders/vrtest.spv").build();
}

void begin_frame() {
	CHK_VK(vkResetCommandPool(logicalDevice, graphicsCommandPool, 0));

	VkCommandBufferBeginInfo cmdBufInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	cmdBufInfo.flags = 0;
	cmdBufInfo.pInheritanceInfo = nullptr;
	CHK_VK(vkBeginCommandBuffer(graphicsCommandBuffer, &cmdBufInfo));

	VkRenderPassBeginInfo renderPassBegin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderPassBegin.renderPass = mainRenderPass;
	renderPassBegin.framebuffer = xrSwapchainData.swapchainFramebuffers[xrSwapchainData.swapchainImageIdx];
	renderPassBegin.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ XR::xrRenderWidth, XR::xrRenderHeight } };
	renderPassBegin.clearValueCount = 1;
	VkClearValue clearValue{};
	clearValue.color.float32[0] = 0.0F;
	clearValue.color.float32[1] = 0.0F;
	clearValue.color.float32[2] = 0.0F;
	clearValue.color.float32[3] = 0.0F;
	renderPassBegin.pClearValues = &clearValue;
	vkCmdBeginRenderPass(graphicsCommandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

}

void end_frame() {
	vkCmdEndRenderPass(graphicsCommandBuffer);

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
	for (u32 i = 0; i < xrSwapchainData.swapchainImageCount; i++) {
		if (xrSwapchainData.swapchainFramebuffers && xrSwapchainData.swapchainFramebuffers[i]) {
			vkDestroyFramebuffer(logicalDevice, xrSwapchainData.swapchainFramebuffers[i], VK_NULL_HANDLE);
		}
	}
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