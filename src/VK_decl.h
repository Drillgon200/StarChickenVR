#pragma once
#include "DrillLib.h"
// The *_decl files are really part of their main files, just including
// some declarations for things because the C++ file dependency system sucks.
#define VK_NO_PROTOTYPES
#define VK_NO_STDINT_H
#define VK_NO_STDDEF_H
#include "../external/vulkan/vulkan.h"
#include "VKGeometry_decl.h"
#include "VKStaging_decl.h"

#define CHK_VK(cmd) if((cmd) != VK_SUCCESS){ ::VK::vulkan_failure(""); }
#define CHK_VK_NOT_NULL(cmd, msg) if((cmd) == nullptr){ ::VK::vulkan_failure(msg); }
#define VK_DEBUG 1
namespace VK {
// Preprocessor hack to make defining and loading functions easier (just add them to the end of this macro when needed)
#define VK_INSTANCE_FUNCTIONS OP(vkGetDeviceProcAddr)\
	OP(vkGetDeviceQueue)\
	OP(vkCreateDebugUtilsMessengerEXT)\
	OP(vkDestroyDebugUtilsMessengerEXT)\
	OP(vkGetPhysicalDeviceQueueFamilyProperties)\
	OP(vkGetPhysicalDeviceSurfaceSupportKHR)\
	OP(vkDestroyInstance)\
	OP(vkDestroyDevice)\
	OP(vkGetPhysicalDeviceMemoryProperties)

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
	OP(vkResetCommandPool)\
	OP(vkQueueWaitIdle)\
	OP(vkCmdBindPipeline)\
	OP(vkCmdDraw)\
	OP(vkDestroyPipeline)\
	OP(vkCreateGraphicsPipelines)\
	OP(vkCreatePipelineLayout)\
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
	OP(vkCmdCopyBufferToImage)\
	OP(vkCmdCopyImageToBuffer)\
	OP(vkCmdPipelineBarrier)\
	OP(vkBindBufferMemory)\
	OP(vkDestroyCommandPool)\
	OP(vkCreateFence)\
	OP(vkWaitForFences)\
	OP(vkCreateComputePipelines)

#define OP(name) extern PFN_##name name;
VK_INSTANCE_FUNCTIONS
VK_DEVICE_FUNCTIONS
#undef OP

struct SwapchainData {
	// Set at init time when the swapchain is created
	VkImage* swapchainImages;
	VkImageView* swapchainImageViews;
	VkFramebuffer* swapchainFramebuffers;
	u32 swapchainImageCount;
	// Set when a swapchain image is acquired
	u32 swapchainImageIdx;
};

#pragma pack(push, 1)
struct PushConstantMatrices {
	Vector4f leftMatrixRow0;
	Vector4f leftMatrixRow1;
	Vector4f leftMatrixRow3;
	Vector4f rightMatrixRow0;
	Vector4f rightMatrixRow1;
	Vector4f rightMatrixRow3;
};
#pragma pack(pop)

extern VkInstance vkInstance;
extern VkPhysicalDevice physicalDevice;
extern VkDevice logicalDevice;

extern u32 hostMemoryTypeIndex;
extern u32 deviceMemoryTypeIndex;

extern u32 graphicsFamily;
extern u32 transferFamily;
extern u32 computeFamily;

extern VkQueue graphicsQueue;
extern VkQueue transferQueue;
extern VkQueue computeQueue;

extern VKStaging::GPUUploadStager graphicsStager;
extern VKGeometry::GeometryHandler geometryHandler;

extern SwapchainData xrSwapchainData;

void vulkan_failure(const char* msg);
}