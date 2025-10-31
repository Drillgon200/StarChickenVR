#pragma once
#include "DrillLib.h"
// The *_decl files are really part of their main files, just including
// some declarations for things because the C++ file dependency system sucks.
#define VK_NO_PROTOTYPES
#define VK_NO_STDINT_H
#define VK_NO_STDDEF_H
#pragma warning(push, 0)
#include "../external/vulkan/vulkan.h"
// From vulkan_win32.h. I didn't want to include that file directly because it includes windows.h and I may want to remove that include eventually.
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
typedef VkFlags VkWin32SurfaceCreateFlagsKHR;
typedef struct VkWin32SurfaceCreateInfoKHR {
	VkStructureType                 sType;
	const void* pNext;
	VkWin32SurfaceCreateFlagsKHR    flags;
	HINSTANCE                       hinstance;
	HWND                            hwnd;
} VkWin32SurfaceCreateInfoKHR;

typedef VkResult(VKAPI_PTR* PFN_vkCreateWin32SurfaceKHR)(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
typedef VkBool32(VKAPI_PTR* PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);
#pragma warning(pop)
#include "VKGeometry_decl.h"
#include "VKStaging_decl.h"

#define CHK_VK(cmd) { VkResult chkVK_Result = cmd; if(chkVK_Result != VK_SUCCESS){ ::VK::vulkan_failure(chkVK_Result, ""); } }
#define CHK_VK_NOT_NULL(cmd, msg) if((cmd) == nullptr){ ::VK::vulkan_failure(VK_SUCCESS, msg); }
#define VK_DEBUG 1
#if VK_DEBUG != 0
#define VK_NAME_OBJ(type, var) {\
		VkDebugUtilsObjectNameInfoEXT nameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };\
		nameInfo.objectType = VK_OBJECT_TYPE_##type;\
		nameInfo.objectHandle = (U64)(var);\
		nameInfo.pObjectName = #var;\
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &nameInfo);\
	}
#else
#define VK_NAME_OBJ(type, var)
#endif
namespace VK {
// Preprocessor hack to make defining and loading functions easier (just add them to the end of this macro when needed)
#define VK_INSTANCE_FUNCTIONS OP(vkGetDeviceProcAddr)\
	OP(vkGetDeviceQueue)\
	OP(vkGetPhysicalDeviceQueueFamilyProperties)\
	OP(vkGetPhysicalDeviceSurfaceSupportKHR)\
	OP(vkDestroyInstance)\
	OP(vkDestroyDevice)\
	OP(vkGetPhysicalDeviceMemoryProperties)\
	OP(vkGetPhysicalDeviceProperties)\
	OP(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)\
	OP(vkGetPhysicalDeviceWin32PresentationSupportKHR)\
	OP(vkGetPhysicalDeviceSurfacePresentModesKHR)\
	OP(vkGetPhysicalDeviceSurfaceFormatsKHR)\
	OP(vkCreateWin32SurfaceKHR)\
	OP(vkDestroySurfaceKHR)\
	OP(vkEnumeratePhysicalDevices)\
	OP(vkGetPhysicalDeviceProperties2)\
	OP(vkGetPhysicalDeviceFeatures2)\
	OP(vkEnumerateDeviceExtensionProperties)\
	OP(vkCreateDevice)

#if VK_DEBUG != 0
#define VK_DEBUG_INSTANCE_FUNCTIONS OP(vkCreateDebugUtilsMessengerEXT)\
	OP(vkDestroyDebugUtilsMessengerEXT)\
	OP(vkCmdBeginDebugUtilsLabelEXT)\
	OP(vkCmdEndDebugUtilsLabelEXT)\
	OP(vkCmdInsertDebugUtilsLabelEXT)\
	OP(vkSetDebugUtilsObjectNameEXT)
#else
#define VK_DEBUG_INSTANCE_FUNCTIONS
#endif

#define VK_DEVICE_FUNCTIONS OP(vkCmdBeginRenderPass)\
	OP(vkCmdEndRenderPass)\
	OP(vkCreateCommandPool)\
	OP(vkAllocateCommandBuffers)\
	OP(vkCreateRenderPass)\
	OP(vkDestroyRenderPass)\
	OP(vkCreateFramebuffer)\
	OP(vkDestroyFramebuffer)\
	OP(vkBeginCommandBuffer)\
	OP(vkEndCommandBuffer)\
	OP(vkQueueSubmit)\
	OP(vkDeviceWaitIdle)\
	OP(vkCreateImageView)\
	OP(vkDestroyImageView)\
	OP(vkResetCommandPool)\
	OP(vkQueueWaitIdle)\
	OP(vkCmdBindPipeline)\
	OP(vkCmdDraw)\
	OP(vkDestroyPipeline)\
	OP(vkCreateGraphicsPipelines)\
	OP(vkCreatePipelineLayout)\
	OP(vkDestroyPipelineLayout)\
	OP(vkCreateShaderModule)\
	OP(vkDestroyShaderModule)\
	OP(vkCmdSetViewport)\
	OP(vkCmdSetScissor)\
	OP(vkCmdPushConstants)\
	OP(vkCreateDescriptorPool)\
	OP(vkCreateDescriptorSetLayout)\
	OP(vkAllocateMemory)\
	OP(vkCreateBuffer)\
	OP(vkCreateBufferView)\
	OP(vkCmdBindVertexBuffers)\
	OP(vkCmdBindIndexBuffer)\
	OP(vkDestroyBuffer)\
	OP(vkFreeMemory)\
	OP(vkMapMemory)\
	OP(vkUnmapMemory)\
	OP(vkFlushMappedMemoryRanges)\
	OP(vkCmdCopyBuffer)\
	OP(vkCmdCopyImage)\
	OP(vkCmdCopyBufferToImage)\
	OP(vkCmdCopyImageToBuffer)\
	OP(vkCmdPipelineBarrier)\
	OP(vkBindBufferMemory)\
	OP(vkDestroyCommandPool)\
	OP(vkCreateSemaphore)\
	OP(vkCreateFence)\
	OP(vkWaitForFences)\
	OP(vkResetFences)\
	OP(vkCreateComputePipelines)\
	OP(vkCmdDrawIndexed)\
	OP(vkCreateImage)\
	OP(vkDestroyImage)\
	OP(vkBindImageMemory)\
	OP(vkGetBufferMemoryRequirements)\
	OP(vkGetImageMemoryRequirements)\
	OP(vkCmdBlitImage)\
	OP(vkAllocateDescriptorSets)\
	OP(vkDestroyDescriptorSetLayout)\
	OP(vkDestroyDescriptorPool)\
	OP(vkCmdBindDescriptorSets)\
	OP(vkCmdDispatch)\
	OP(vkUpdateDescriptorSets)\
	OP(vkQueuePresentKHR)\
	OP(vkGetFenceStatus)\
	OP(vkAcquireNextImageKHR)\
	OP(vkCreateSwapchainKHR)\
	OP(vkDestroySwapchainKHR)\
	OP(vkGetSwapchainImagesKHR)\
	OP(vkDestroyFence)\
	OP(vkDestroySemaphore)\
	OP(vkCreateSampler)\
	OP(vkDestroySampler)\
	OP(vkGetBufferDeviceAddress)\
	OP(vkCmdUpdateBuffer)

#if VK_DEBUG != 0
#define VK_DEBUG_DEVICE_FUNCTIONS
#else
#define VK_DEBUG_DEVICE_FUNCTIONS
#endif

#define OP(name) extern PFN_##name name;
VK_INSTANCE_FUNCTIONS
VK_DEBUG_INSTANCE_FUNCTIONS
VK_DEVICE_FUNCTIONS
VK_DEBUG_DEVICE_FUNCTIONS
#undef OP

const U32 FRAMES_IN_FLIGHT = 2;

extern U32 currentFrameInFlight;

const U32 VERTEX_FORMAT_POS3F_TEX2F_NORM3F_TAN3F_SIZE = sizeof(V3F32) + sizeof(V2F32) + sizeof(V3F32) + sizeof(V3F32);
const U32 VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE = sizeof(U8) * 4 + sizeof(U8) * 4;
const U32 VERTEX_FORMAT_POS3F_NORM3F_TAN3F_SIZE = sizeof(V3F32) + sizeof(V3F32) + sizeof(V3F32);

const U32 MAX_MIP_LEVEL = 17; // We will never have a texture bigger than 16k

struct XrSwapchainData {
	// Set at init time when the swapchain is created
	VkImage* swapchainColorImages;
	VkImage* swapchainDepthImages;
	U32 swapchainColorImageCount;
	U32 swapchainDepthImageCount;
	// Set when a swapchain image is acquired
	U32 swapchainColorImageIdx;
	U32 swapchainDepthImageIdx;
};

extern VkInstance vkInstance;
extern VkPhysicalDevice physicalDevice;
extern VkDevice logicalDevice;

extern U32 hostMemoryTypeIndex;
extern U32 deviceMemoryTypeIndex;
extern VkMemoryPropertyFlags hostMemoryFlags;
extern VkMemoryPropertyFlags deviceMemoryFlags;

extern VkPhysicalDeviceProperties physicalDeviceProperties;

extern U32 graphicsFamily;
extern U32 transferFamily;
extern U32 computeFamily;

extern VkQueue graphicsQueue;
extern VkQueue transferQueue;
extern VkQueue computeQueue;

extern VKStaging::GPUUploadStager graphicsStager;
extern VKGeometry::GeometryHandler geometryHandler;
extern VKGeometry::UniformMatricesHandler uniformMatricesHandler;

void vulkan_failure(VkResult result, const char* msg);

struct DedicatedBuffer {
	VkDeviceMemory memory;
	VkBuffer buffer;
	void* mapping;
	VkDeviceAddress gpuAddress;
	U64 capacity;

	void create(U64 size, VkBufferUsageFlags usage, U32 memoryTypeIndex) {
		capacity = size;
		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.flags = 0;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
		CHK_VK(vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer));

		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);
		if (!(memoryRequirements.memoryTypeBits & 1 << memoryTypeIndex)) {
			abort("Could not create buffer in correct memory type");
		}

		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = memoryTypeIndex;
		VkMemoryAllocateFlagsInfo allocFlags{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
		if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
			allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
		} else {
			allocFlags.flags = 0;
		}
		allocFlags.deviceMask = 0;
		allocInfo.pNext = &allocFlags;
		VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
		dedicatedAllocInfo.buffer = buffer;
		allocFlags.pNext = &dedicatedAllocInfo;
		CHK_VK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &memory));
		CHK_VK(vkBindBufferMemory(logicalDevice, buffer, memory, 0));

		if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
			VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
			addressInfo.buffer = buffer;
			gpuAddress = vkGetBufferDeviceAddress(logicalDevice, &addressInfo);
		} else {
			gpuAddress = 0;
		}

		if (memoryTypeIndex == hostMemoryTypeIndex) {
			CHK_VK(vkMapMemory(logicalDevice, memory, 0, capacity, 0, reinterpret_cast<void**>(&mapping)));
		}
	}
	void destroy() {
		if (memory != VK_NULL_HANDLE) {
			if (mapping) {
				vkUnmapMemory(logicalDevice, memory);
			}
			vkDestroyBuffer(logicalDevice, buffer, nullptr);
			vkFreeMemory(logicalDevice, memory, nullptr);
			memory = VK_NULL_HANDLE;
			buffer = VK_NULL_HANDLE;
			mapping = nullptr;
			gpuAddress = 0;
			capacity = 0;
		}
	}
	void invalidate_mapped(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) {
		if (mapping && !(VK::hostMemoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			VkMappedMemoryRange memoryInvalidateRange{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			memoryInvalidateRange.memory = memory;
			memoryInvalidateRange.offset = offset;
			memoryInvalidateRange.size = size;
			CHK_VK(VK::vkFlushMappedMemoryRanges(VK::logicalDevice, 1, &memoryInvalidateRange));
		}
	}
};

#pragma pack(push, 1)
struct GPUModelInfo {
	U32 transformIdx;
	I32 verticesOffset;
};
struct CubemapPipelineInfo {
	V2U inputDim;
	V2U outputDim;
	F32 roughness;
	U32 outputIdx;
	U32 inputIdx;
};
#pragma pack(pop)

struct FramebufferAttachment {
	VkDeviceMemory memory;
	VkImage image;
	VkImageView imageView;
	VkFormat imageFormat;
	VkImageUsageFlags imageUsage;
	VkImageAspectFlags imageAspectMask;
	U32 layerCount;
	B32 ownsImage;
};

struct Framebuffer {
	static constexpr U32 MAX_FRAMEBUFFER_ATTACHMENTS = 4;
	VkFramebuffer framebuffer;
	VkRenderPass renderPass;
	U32 framebufferWidth;
	U32 framebufferHeight;
	FramebufferAttachment attachments[MAX_FRAMEBUFFER_ATTACHMENTS];
	U32 attachmentCount;
	B32 addedToFreeList;

	Framebuffer& set_default();
	Framebuffer& render_pass(VkRenderPass pass);
	Framebuffer& dimensions(U32 width, U32 height);
	Framebuffer& new_attachment(VkFormat imageFormat, VkImageUsageFlags usage, VkImageAspectFlags aspectMask, U32 layerCount);
	Framebuffer& existing_attachment(VkImageView view);
	Framebuffer& build();
	void destroy();
};

struct DescriptorSet {
	// I'm just going to always create these together.
	// I won't have that many descriptor sets because of the bindless architecture, and this simplifies a lot
	VkDescriptorSetLayout setLayout;
	VkDescriptorSet descriptorSet;
	VkDescriptorPool descriptorPool;

	static constexpr U32 MAX_LAYOUT_BINDINGS = 16;
	VkDescriptorSetLayoutBinding bindings[MAX_LAYOUT_BINDINGS];
	VkDescriptorBindingFlags bindingFlags[MAX_LAYOUT_BINDINGS];
	U32 bindingCount;
	U32 variableDescriptorCountMax;
	B32 updateAfterBind;

	DescriptorSet& init();

	DescriptorSet& basic_binding(U32 bindingIndex, VkShaderStageFlags stageFlags, VkDescriptorType descriptorType, U32 descriptorCount, VkDescriptorBindingFlags flags);
	DescriptorSet& storage_buffer(U32 bindingIndex, VkShaderStageFlags stageFlags);
	DescriptorSet& uniform_buffer(U32 bindingIndex, VkShaderStageFlags stageFlags);
	DescriptorSet& sampler(U32 bindingIndex, VkShaderStageFlags stageFlags);
	DescriptorSet& sampled_image_array_dynamic(U32 bindingIndex, VkShaderStageFlags stageFlags, U32 maxArraySize, U32 arraySize);
	DescriptorSet& sampled_image_array(U32 bindingIndex, VkShaderStageFlags stageFlags, U32 arraySize);
	DescriptorSet& storage_image_array(U32 bindingIndex, VkShaderStageFlags stageFlags, U32 arraySize);
	DescriptorSet& storage_image(U32 bindingIdx, VkShaderStageFlags stageFlags);
	DescriptorSet& sampled_image(U32 bindingIdx, VkShaderStageFlags stageFlags);

	void build();

	// Should only be called before rendering anything in the current frame
	void change_dynamic_array_length(U32 newDescriptorCount);

	U32 current_dynamic_array_length();
};

extern Framebuffer mainFramebuffer;

extern XrSwapchainData xrSwapchainData;

}