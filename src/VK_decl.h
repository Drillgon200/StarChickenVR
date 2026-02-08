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

#define VK_DEVICE_FUNCTIONS OP(vkCreateCommandPool)\
	OP(vkAllocateCommandBuffers)\
	OP(vkCreateFramebuffer)\
	OP(vkDestroyFramebuffer)\
	OP(vkBeginCommandBuffer)\
	OP(vkEndCommandBuffer)\
	OP(vkQueueSubmit)\
	OP(vkDeviceWaitIdle)\
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
	OP(vkCmdPushDataEXT)\
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
	OP(vkCmdDispatch)\
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
	OP(vkCmdUpdateBuffer)\
	OP(vkCmdBeginRenderingKHR)\
	OP(vkCmdEndRenderingKHR)\
	OP(vkCmdClearAttachments)\
	OP(vkWriteSamplerDescriptorsEXT)\
	OP(vkWriteResourceDescriptorsEXT)\
	OP(vkCmdBindSamplerHeapEXT)\
	OP(vkCmdBindResourceHeapEXT)\
	OP(vkCreateImageView)\
	OP(vkDestroyImageView)

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

#define VK_PUSH_MEMBER(cmdBuf, pushData, memberName) VK::vkCmdPushDataEXT(cmdBuf, ptr(VkPushDataInfoEXT{ .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT, .offset = OFFSET_OF(decltype(pushData), memberName), .data = VkHostAddressRangeConstEXT{ &(pushData).memberName, sizeof((pushData).memberName) } }))
#define VK_PUSH_STRUCT(cmdBuf, pushData, offsetBytes) VK::vkCmdPushDataEXT(cmdBuf, ptr(VkPushDataInfoEXT{ .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT, .offset = (offsetBytes), .data = VkHostAddressRangeConstEXT{ &(pushData), sizeof(pushData) } }))

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
extern U32 hostCachedMemoryTypeIndex;
extern U32 deviceMemoryTypeIndex;
extern U32 deviceHostMappedMemoryTypeIndex;
extern VkMemoryPropertyFlags hostMemoryFlags;
extern VkMemoryPropertyFlags hostCachedMemoryFlags;
extern VkMemoryPropertyFlags deviceMemoryFlags;
extern VkMemoryPropertyFlags deviceHostMappedMemoryFlags;

extern VkPhysicalDeviceProperties2 physicalDeviceProperties;
extern VkPhysicalDeviceDescriptorHeapPropertiesEXT descriptorHeapProperties;

extern U32 graphicsFamily;
extern U32 transferFamily;
extern U32 computeFamily;

extern VkQueue graphicsQueue;
extern VkQueue transferQueue;
extern VkQueue computeQueue;

extern VKStaging::GPUUploadStager graphicsStager;
extern VKGeometry::GeometryHandler geometryHandler;
extern VKGeometry::UniformMatricesHandler uniformMatricesHandler;

extern B32 hasCubemap;

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

		if (memoryTypeIndex == hostMemoryTypeIndex || memoryTypeIndex == hostCachedMemoryTypeIndex || memoryTypeIndex == deviceHostMappedMemoryTypeIndex) {
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

struct DescriptorHeap {
	U32 nextDescriptor;
	U32 capacity;
	U32 samplerNextDescriptor;
	U32 samplerCapacity;
	ArenaArrayList<U32> freeDescriptorSlots;
	U32 descriptorSizeBytes;
	U32 samplerSizeBytes;
	Byte* hostHeap;
	Byte* hostSamplerHeap;
	DedicatedBuffer deviceHeap;
	U32 deviceHeapReservedRangeSize;
	DedicatedBuffer deviceSamplerHeap;
	U32 deviceSamplerHeapReservedRangeSize;

	void init(U32 initialCapacity, U32 initialSamplerCapacity) {
		capacity = initialCapacity;
		samplerCapacity = initialSamplerCapacity;
		descriptorSizeBytes = max(descriptorHeapProperties.imageDescriptorSize, descriptorHeapProperties.bufferDescriptorSize);
		samplerSizeBytes = max(descriptorHeapProperties.samplerDescriptorSize, descriptorHeapProperties.samplerDescriptorAlignment);
		deviceHeapReservedRangeSize = descriptorHeapProperties.minResourceHeapReservedRange;
		deviceSamplerHeapReservedRangeSize = descriptorHeapProperties.minSamplerHeapReservedRange;
		
		hostHeap = globalArena.zalloc_aligned<Byte>(capacity * descriptorSizeBytes, max(descriptorHeapProperties.bufferDescriptorAlignment, descriptorHeapProperties.imageDescriptorAlignment));
		hostSamplerHeap = globalArena.zalloc_aligned<Byte>(samplerCapacity * descriptorSizeBytes, descriptorHeapProperties.samplerDescriptorAlignment);
		deviceHeap.create(capacity * descriptorSizeBytes + deviceHeapReservedRangeSize * FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceHostMappedMemoryTypeIndex);
		deviceSamplerHeap.create(samplerCapacity * samplerSizeBytes + deviceSamplerHeapReservedRangeSize * FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceHostMappedMemoryTypeIndex);
	}

	void destroy() {
		deviceHeap.destroy();
		deviceSamplerHeap.destroy();
	}

	U32 alloc_slot() {
		if (freeDescriptorSlots.empty()) {
			//TODO resize
			RUNTIME_ASSERT(nextDescriptor < capacity, "Need more descriptors");
			return nextDescriptor++;
		} else {
			return freeDescriptorSlots.pop_back();
		}
	}
	void free_slot(U32 slot) {
		freeDescriptorSlots.push_back(slot);
	}

	void bind(VkCommandBuffer cmdBuf) {
		vkCmdBindSamplerHeapEXT(cmdBuf, ptr(VkBindHeapInfoEXT{
			.sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
			.heapRange = VkDeviceAddressRangeEXT{ deviceSamplerHeap.gpuAddress, deviceSamplerHeap.capacity },
			.reservedRangeOffset = samplerCapacity * samplerSizeBytes + currentFrameInFlight * deviceSamplerHeapReservedRangeSize,
			.reservedRangeSize = deviceSamplerHeapReservedRangeSize
		}));
		vkCmdBindResourceHeapEXT(cmdBuf, ptr(VkBindHeapInfoEXT{
			.sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
			.heapRange = VkDeviceAddressRangeEXT{ deviceHeap.gpuAddress, deviceHeap.capacity },
			.reservedRangeOffset = capacity * descriptorSizeBytes + currentFrameInFlight * deviceHeapReservedRangeSize,
			.reservedRangeSize = deviceHeapReservedRangeSize
		}));
	}

	U32 make_uniform_buffer(VkDeviceAddress address, VkDeviceSize size) {
		U32 slot = samplerNextDescriptor++;
		RUNTIME_ASSERT(slot < capacity, "Need more resource slots");
		VkResourceDescriptorInfoEXT buffer{
			.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.data = VkResourceDescriptorDataEXT{
				.pAddressRange = ptr(VkDeviceAddressRangeEXT{ address, size })
			},
		};
		CHK_VK(vkWriteResourceDescriptorsEXT(logicalDevice, 1, &buffer, ptr(VkHostAddressRangeEXT{ hostHeap + slot * descriptorSizeBytes, descriptorSizeBytes })));
		memcpy((Byte*)deviceHeap.mapping + slot * descriptorSizeBytes, hostHeap + slot * descriptorSizeBytes, descriptorSizeBytes);
		return slot;
	}

	U32 make_image(VkImage img, VkImageViewType viewType, VkFormat format, VkDescriptorType type, VkImageAspectFlags aspect, U32 mipLevel, U32 levelCount, U32 baseArrayLayer, U32 layerCount, VkImageUsageFlags separateUsage) {
		U32 slot = samplerNextDescriptor++;
		RUNTIME_ASSERT(slot < capacity, "Need more resource slots");
		VkImageViewUsageCreateInfo viewUsage{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
			.usage = separateUsage
		};
		VkResourceDescriptorInfoEXT sampledImage{
			.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
			.type = type,
			.data = VkResourceDescriptorDataEXT{
				.pImage = ptr(VkImageDescriptorInfoEXT{
					.sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT,
					.pView = ptr(VkImageViewCreateInfo{
						.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
						.pNext = separateUsage == 0 ? nullptr : &viewUsage,
						.image = img,
						.viewType = viewType,
						.format = format,
						.subresourceRange = VkImageSubresourceRange{
							.aspectMask = aspect,
							.baseMipLevel = mipLevel,
							.levelCount = levelCount,
							.baseArrayLayer = baseArrayLayer,
							.layerCount = layerCount
						}
					}),
					.layout = VK_IMAGE_LAYOUT_GENERAL
				})
			}
		};
		CHK_VK(vkWriteResourceDescriptorsEXT(logicalDevice, 1, &sampledImage, ptr(VkHostAddressRangeEXT{ hostHeap + slot * descriptorSizeBytes, descriptorSizeBytes })));
		memcpy((Byte*)deviceHeap.mapping + slot * descriptorSizeBytes, hostHeap + slot * descriptorSizeBytes, descriptorSizeBytes);
		return slot;
	}

	U32 make_sampled_image(VkImage img, VkImageViewType viewType, VkFormat format) {
		return make_image(img, viewType, format, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS, 0);
	}

	U32 make_storage_image(VkImage img, VkImageViewType viewType, VkFormat format) {
		return make_image(img, viewType, format, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS, 0);
	}

	U32 make_sampler(VkFilter filter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode, F32 anisotropy) {
		anisotropy = clamp(anisotropy, 1.0F, physicalDeviceProperties.properties.limits.maxSamplerAnisotropy);
		VkSamplerCreateInfo samplerInfo{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = filter,
			.minFilter = filter,
			.mipmapMode = mipmapMode,
			.addressModeU = addressMode,
			.addressModeV = addressMode,
			.addressModeW = addressMode,
			.anisotropyEnable = anisotropy == 1.0F ? VK_FALSE : VK_TRUE,
			.maxAnisotropy = anisotropy,
			.minLod = 0.0F,
			.maxLod = VK_LOD_CLAMP_NONE,
		};
		U32 samplerSlot = samplerNextDescriptor++;
		RUNTIME_ASSERT(samplerSlot < samplerCapacity, "Need more samplers");
		CHK_VK(vkWriteSamplerDescriptorsEXT(logicalDevice, 1, &samplerInfo, ptr(VkHostAddressRangeEXT{ hostSamplerHeap + samplerSlot * samplerSizeBytes, samplerSizeBytes })));
		memcpy((Byte*)deviceSamplerHeap.mapping + samplerSlot * samplerSizeBytes, hostSamplerHeap + samplerSlot * samplerSizeBytes, samplerSizeBytes);
		return samplerSlot;
	}
} descriptorHeap;

enum RenderPass {
	RENDER_PASS_WORLD,
	RENDER_PASS_WORLD_NO_ID,
	RENDER_PASS_UI
};

#pragma pack(push, 1)
struct GPUCameraMatrices {
	M4x3F worldToView;
	F32 projXScale;
	F32 projXZBias;
	F32 projYScale;
	F32 projYZBias;
	V3F position;
	V3F direction;
};
struct WorldDrawConstants {
	U32 transformIdx;
	I32 verticesOffset;
	U32 camIdx;
	U32 objId; // High bit set if object is selected
	U32 materialId;
};
struct DrawDescriptorSet {
	U32 drawDataUniformBuffer;
	U32 specularCubemap;
	U32 diffuseCubemap;
	U32 brdfLUT;
} defaultDrawDescriptorSet;
struct DrawPushData {
	DrawDescriptorSet drawSet;
	WorldDrawConstants drawConstants;
};
struct BackgroundPushData {
	DrawDescriptorSet drawSet;
	U32 camIdx;
};
struct FinalCompositePushData {
	U32 sceneColor;
	U32 sceneIds;
	U32 uiColor;
	U32 compositeOutput;
	U32 activeObjectId;
	V2U outputDimensions;
};
struct DrawDataUniforms {
	V2F screenDimensions;
	UPtr uiClipBoxes;
	UPtr uiVertices;
	UPtr matrices;
	UPtr positions;
	UPtr texcoords;
	UPtr normals;
	UPtr tangents;
	UPtr boneIndicesAndWeights;
	UPtr skinnedPositions;
	UPtr skinnedNormals;
	UPtr skinnedTangents;
	UPtr materials;
	UPtr cameras;
};
#pragma pack(pop)

struct DedicatedImage {
	VkImage img;
	VkImageView imgView;
	U32 descriptorIdx;
	VkDeviceMemory memory;
};

extern XrSwapchainData xrSwapchainData;

}