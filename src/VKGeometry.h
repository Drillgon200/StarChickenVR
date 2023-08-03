#pragma once
#include "DrillLib.h"
#include "VK_decl.h"

namespace VKGeometry {

struct GeometryAllocation {
	VkBuffer buffer;
	u64 offset;
	u64 size;
};

struct GeometryHandler {
	VkDeviceMemory memory;
	VkDeviceSize memorySize;
	VkBuffer buffer;
	u64 offset;

	void init(VkDeviceSize size) {
		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = size;
		allocInfo.memoryTypeIndex = VK::deviceMemoryTypeIndex;
		CHK_VK(VK::vkAllocateMemory(VK::logicalDevice, &allocInfo, nullptr, &memory));
		memorySize = size;
		offset = 0;

		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.flags = 0;
		bufferInfo.size = size;
		bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = 1;
		bufferInfo.pQueueFamilyIndices = &VK::graphicsFamily;
		CHK_VK(VK::vkCreateBuffer(VK::logicalDevice, &bufferInfo, VK_NULL_HANDLE, &buffer));
	}

	GeometryAllocation alloc(u64 size, u32 alignment) {
		offset = ALIGN_HIGH(offset, alignment);
		GeometryAllocation allocation{ buffer, offset, size };
		offset += size;
		if (offset > memorySize) {
			abort("Ran out of memory for geometry buffer. Perhaps time to implement streaming?");
		}
		return allocation;
	}

	void destroy() {
		VK::vkDestroyBuffer(VK::logicalDevice, buffer, VK_NULL_HANDLE);
		VK::vkFreeMemory(VK::logicalDevice, memory, nullptr);
	}
};

}