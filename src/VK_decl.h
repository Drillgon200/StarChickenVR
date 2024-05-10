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
	OP(vkGetPhysicalDeviceProperties)\
	OP(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)\
	OP(vkGetPhysicalDeviceWin32PresentationSupportKHR)\
	OP(vkGetPhysicalDeviceSurfacePresentModesKHR)\
	OP(vkGetPhysicalDeviceSurfaceFormatsKHR)\
	OP(vkCreateWin32SurfaceKHR)\
	OP(vkDestroySurfaceKHR)

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
	OP(vkDestroySemaphore)


#define OP(name) extern PFN_##name name;
VK_INSTANCE_FUNCTIONS
VK_DEVICE_FUNCTIONS
#undef OP

static constexpr U32 VERTEX_FORMAT_POS3F_TEX2F_NORM3F_TAN3F_SIZE = sizeof(V3F32) + sizeof(V2F32) + sizeof(V3F32) + sizeof(V3F32);
static constexpr U32 VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE = sizeof(U8) * 4 + sizeof(U8) * 4;
static constexpr U32 VERTEX_FORMAT_POS3F_NORM3F_TAN3F_SIZE = sizeof(V3F32) + sizeof(V3F32) + sizeof(V3F32);

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

#pragma pack(push, 1)
struct GPUModelInfo {
	U32 transformIdx;
};
#pragma pack(pop)

extern VkInstance vkInstance;
extern VkPhysicalDevice physicalDevice;
extern VkDevice logicalDevice;

struct FramebufferAttachment {
	VkDeviceMemory memory;
	VkImage image;
	VkImageView imageView;
	B32 ownsImageView;
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

extern Framebuffer mainFramebuffer;

extern U32 hostMemoryTypeIndex;
extern U32 deviceMemoryTypeIndex;

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

extern XrSwapchainData xrSwapchainData;

void vulkan_failure(VkResult result, const char* msg);

}