#pragma once
#include "DrillLib.h"
// The *_decl files are really part of their main files, just including
// some declarations for things because the C++ file dependency system sucks.
#define VK_NO_PROTOTYPES
#define VK_NO_STDINT_H
#define VK_NO_STDDEF_H
#pragma warning(push, 0)
#include "../external/vulkan/vulkan.h"
#pragma warning(pop)
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
	OP(vkGetPhysicalDeviceMemoryProperties)\
	OP(vkGetPhysicalDeviceProperties)

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
	OP(vkUpdateDescriptorSets)

#define OP(name) extern PFN_##name name;
VK_INSTANCE_FUNCTIONS
VK_DEVICE_FUNCTIONS
#undef OP

static constexpr u32 VERTEX_FORMAT_POS3F_TEX2F_NORM3F_TAN3F_SIZE = sizeof(Vector3f) + sizeof(Vector2f) + sizeof(Vector3f) + sizeof(Vector3f);
static constexpr u32 VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE = sizeof(u8) * 4 + sizeof(u8) * 4;
static constexpr u32 VERTEX_FORMAT_POS3F_NORM3F_TAN3F_SIZE = sizeof(Vector3f) + sizeof(Vector3f) + sizeof(Vector3f);

struct SwapchainData {
	// Set at init time when the swapchain is created
	VkImage* swapchainImages;
	VkImageView* swapchainImageViews;
	//VkFramebuffer* swapchainFramebuffers;
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

	void set_eye_transforms(ProjectiveTransformMatrix& left, ProjectiveTransformMatrix& right) {
		leftMatrixRow0 = Vector4f{ left.m00, left.m01, left.m02, left.m03 };
		leftMatrixRow1 = Vector4f{ left.m10, left.m11, left.m12, left.m13 };
		leftMatrixRow3 = Vector4f{ left.m30, left.m31, left.m32, left.m33 };
		rightMatrixRow0 = Vector4f{ right.m00, right.m01, right.m02, right.m03 };
		rightMatrixRow1 = Vector4f{ right.m10, right.m11, right.m12, right.m13 };
		rightMatrixRow3 = Vector4f{ right.m30, right.m31, right.m32, right.m33 };
	}
};
#pragma pack(pop)

extern VkInstance vkInstance;
extern VkPhysicalDevice physicalDevice;
extern VkDevice logicalDevice;

struct FramebufferAttachment {
	VkDeviceMemory memory;
	VkImage image;
	VkImageView imageView;
	b32 ownsImageView;
};

struct Framebuffer {
	static constexpr u32 MAX_FRAMEBUFFER_ATTACHMENTS = 4;
	VkFramebuffer framebuffer;
	VkRenderPass renderPass;
	u32 framebufferWidth;
	u32 framebufferHeight;
	FramebufferAttachment attachments[MAX_FRAMEBUFFER_ATTACHMENTS];
	u32 attachmentCount;

	Framebuffer& set_default();
	Framebuffer& render_pass(VkRenderPass pass);
	Framebuffer& dimensions(u32 width, u32 height);
	Framebuffer& new_attachment(VkFormat imageFormat, VkImageUsageFlags usage, VkImageAspectFlags aspectMask, u32 layerCount);
	Framebuffer& existing_attachment(VkImageView view);
	Framebuffer& build();
	void destroy();
};

extern Framebuffer mainFramebuffer;

extern u32 hostMemoryTypeIndex;
extern u32 deviceMemoryTypeIndex;

extern VkPhysicalDeviceProperties physicalDeviceProperties;

extern u32 graphicsFamily;
extern u32 transferFamily;
extern u32 computeFamily;

extern VkQueue graphicsQueue;
extern VkQueue transferQueue;
extern VkQueue computeQueue;

extern VKStaging::GPUUploadStager graphicsStager;
extern VKGeometry::GeometryHandler geometryHandler;
extern VKGeometry::SkinningHandler skinningHandler;

extern SwapchainData xrSwapchainData;

void vulkan_failure(const char* msg);
}