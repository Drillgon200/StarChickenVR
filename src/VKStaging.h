#pragma once
#include "DrillLib.h"
#include "VK_decl.h"

namespace VKStaging {

struct StagingBuffer {
	VkBuffer uploadBuffer;
	void* memoryMapping;
	VkCommandPool commandPool;
	VkCommandBuffer cmdBuffer;
	VkFence uploadFinishedFence;
	u32 offset;
	b32 submitted;
};

struct GPUUploadStager {
	static constexpr u32 UPLOAD_BUFFER_SIZE = 8 * ONE_MEGABYTE;

	VkQueue queue;
	VkDeviceMemory memory;
	StagingBuffer stagingBuffers[2];
	u32 currentBufferIdx;

	void init(VkQueue queueToUse, u32 queueFamilyIdx) {
		queue = queueToUse;
		currentBufferIdx = 0;

		VkMemoryAllocateInfo memoryAllocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		memoryAllocateInfo.allocationSize = 2 * UPLOAD_BUFFER_SIZE;
		memoryAllocateInfo.memoryTypeIndex = VK::hostMemoryTypeIndex;
		CHK_VK(VK::vkAllocateMemory(VK::logicalDevice, &memoryAllocateInfo, VK_NULL_HANDLE, &memory));
		void* memoryMapping;
		CHK_VK(VK::vkMapMemory(VK::logicalDevice, memory, 0, UPLOAD_BUFFER_SIZE, 0, &memoryMapping));
		for (u32 i = 0; i < 2; i++) {
			StagingBuffer& stagingBuffer = stagingBuffers[i];
			stagingBuffer.memoryMapping = reinterpret_cast<byte*>(memoryMapping) + i * UPLOAD_BUFFER_SIZE;
			VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bufferInfo.flags = 0;
			bufferInfo.size = UPLOAD_BUFFER_SIZE;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bufferInfo.queueFamilyIndexCount = 1;
			bufferInfo.pQueueFamilyIndices = &queueFamilyIdx;
			CHK_VK(VK::vkCreateBuffer(VK::logicalDevice, &bufferInfo, VK_NULL_HANDLE, &stagingBuffer.uploadBuffer));
			CHK_VK(VK::vkBindBufferMemory(VK::logicalDevice, stagingBuffer.uploadBuffer, memory, i * UPLOAD_BUFFER_SIZE));

			VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			cmdPoolInfo.flags = 0;
			cmdPoolInfo.queueFamilyIndex = queueFamilyIdx;
			CHK_VK(VK::vkCreateCommandPool(VK::logicalDevice, &cmdPoolInfo, VK_NULL_HANDLE, &stagingBuffer.commandPool));
			VkCommandBufferAllocateInfo cmdBufInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			cmdBufInfo.commandPool = stagingBuffer.commandPool;
			cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cmdBufInfo.commandBufferCount = 1;
			CHK_VK(VK::vkAllocateCommandBuffers(VK::logicalDevice, &cmdBufInfo, &stagingBuffer.cmdBuffer));

			VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			fenceInfo.flags = 0;
			CHK_VK(VK::vkCreateFence(VK::logicalDevice, &fenceInfo, VK_NULL_HANDLE, &stagingBuffer.uploadFinishedFence));
		}
		
	}

	void destroy() {
		for (u32 i = 0; i < 2; i++) {
			VK::vkDestroyCommandPool(VK::logicalDevice, stagingBuffers[i].commandPool, VK_NULL_HANDLE);

			VK::vkUnmapMemory(VK::logicalDevice, memory);
			VK::vkDestroyBuffer(VK::logicalDevice, stagingBuffers[i].uploadBuffer, VK_NULL_HANDLE);
		}
		VK::vkFreeMemory(VK::logicalDevice, memory, VK_NULL_HANDLE);
	}

	VkCommandBuffer current_cmd_buf() {
		return stagingBuffers[currentBufferIdx].cmdBuffer;
	}

	void upload_to_buffer(VkBuffer dst, u64 dstOffset, void* src, u64 size) {
		while (size) {
			StagingBuffer& stagingBuffer = stagingBuffers[currentBufferIdx];
			wait_for_staging_buffer(stagingBuffer);
			u32 amountToCopy = min<u64>(size, UPLOAD_BUFFER_SIZE - stagingBuffer.offset);
			memcpy(reinterpret_cast<byte*>(stagingBuffer.memoryMapping) + stagingBuffer.offset, src, amountToCopy);
			VkBufferCopy region{};
			region.srcOffset = stagingBuffer.offset;
			region.dstOffset = dstOffset;
			region.size = amountToCopy;
			VK::vkCmdCopyBuffer(stagingBuffer.cmdBuffer, stagingBuffer.uploadBuffer, dst, 1, &region);
			stagingBuffer.offset += amountToCopy;
			dstOffset += amountToCopy;
			src = reinterpret_cast<byte*>(src) + amountToCopy;
			size -= amountToCopy;
			if (stagingBuffer.offset == UPLOAD_BUFFER_SIZE) {
				flush();
			}
		}
	}

	void flush() {
		StagingBuffer& stagingBuffer = stagingBuffers[currentBufferIdx];
		if (!stagingBuffer.submitted && stagingBuffer.offset != 0) {
			VkMappedMemoryRange memoryInvalidateRange{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			memoryInvalidateRange.memory = memory;
			memoryInvalidateRange.offset = currentBufferIdx * UPLOAD_BUFFER_SIZE;
			memoryInvalidateRange.size = stagingBuffer.offset;
			CHK_VK(VK::vkFlushMappedMemoryRanges(VK::logicalDevice, 1, &memoryInvalidateRange));

			VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.pWaitDstStageMask = nullptr;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &stagingBuffer.cmdBuffer;
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			CHK_VK(VK::vkQueueSubmit(queue, 1, &submitInfo, stagingBuffer.uploadFinishedFence));

			stagingBuffer.submitted = true;
			currentBufferIdx = (currentBufferIdx + 1) & 1;
		}
	}

	void wait_for_staging_buffer(StagingBuffer& stagingBuffer) {
		if (stagingBuffer.submitted) {
			CHK_VK(VK::vkWaitForFences(VK::logicalDevice, 1, &stagingBuffer.uploadFinishedFence, VK_TRUE, U64_MAX));
			CHK_VK(VK::vkResetCommandPool(VK::logicalDevice, stagingBuffer.commandPool, 0));
			stagingBuffer.submitted = false;
			stagingBuffer.offset = 0;
		}
	}
};

}