#pragma once

#include "DrillLib.h"
#include "VK.h"
#include "ResourceLoading.h"

namespace CubemapGen {

VkCommandPool convolveCommandPool;
VkCommandBuffer convolveCmdBuf;

VkFence convolveCompleteFence;

VK::DedicatedBuffer imageCPUBuffer;

VkImage cubeImg;
VkImageView cubeImgView;
VkDeviceMemory equirectImageMemory;
VkImage equirectImage;
VkImageView equirectImageView;

const U32 CUBEMAP_RES = 512;

void init() {
	imageCPUBuffer.create(2048 * 2048 * 4 * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK::hostMemoryTypeIndex);

	{ // Create cubemap image
		VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		imgInfo.imageType = VK_IMAGE_TYPE_2D;
		imgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imgInfo.extent = VkExtent3D{ CUBEMAP_RES, CUBEMAP_RES, 1 };
		imgInfo.mipLevels = 1;
		imgInfo.arrayLayers = 6;
		imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		CHK_VK(VK::vkCreateImage(VK::logicalDevice, &imgInfo, nullptr, &cubeImg));

		VkMemoryRequirements memoryRequirements;
		VK::vkGetImageMemoryRequirements(VK::logicalDevice, cubeImg, &memoryRequirements);
		VkDeviceMemory linearBlock;
		VkDeviceSize memoryOffset;
		ResourceLoading::alloc_texture_memory(&linearBlock, &memoryOffset, memoryRequirements);
		CHK_VK(VK::vkBindImageMemory(VK::logicalDevice, cubeImg, linearBlock, memoryOffset));

		VkImageViewCreateInfo imgViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		imgViewInfo.image = cubeImg;
		imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		imgViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imgViewInfo.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgViewInfo.subresourceRange.baseMipLevel = 0;
		imgViewInfo.subresourceRange.levelCount = 1;
		imgViewInfo.subresourceRange.baseArrayLayer = 0;
		imgViewInfo.subresourceRange.layerCount = 6;
		CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imgViewInfo, nullptr, &cubeImgView));
	}

	VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolInfo.queueFamilyIndex = VK::graphicsFamily;
	CHK_VK(VK::vkCreateCommandPool(VK::logicalDevice, &poolInfo, nullptr, &convolveCommandPool));
	VkCommandBufferAllocateInfo cmdBufInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmdBufInfo.commandPool = convolveCommandPool;
	cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufInfo.commandBufferCount = 1;
	CHK_VK(VK::vkAllocateCommandBuffers(VK::logicalDevice, &cmdBufInfo, &convolveCmdBuf));

	VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	CHK_VK(VK::vkCreateFence(VK::logicalDevice, &fenceInfo, nullptr, &convolveCompleteFence));
}

void destroy() {
	VK::vkDestroyFence(VK::logicalDevice, convolveCompleteFence, nullptr);
	VK::vkDestroyCommandPool(VK::logicalDevice, convolveCommandPool, nullptr);
	VK::vkDestroyImageView(VK::logicalDevice, cubeImgView, nullptr);
	VK::vkDestroyImage(VK::logicalDevice, cubeImg, nullptr);
	imageCPUBuffer.destroy();
}

void equirectangular2convolved_cubemap(StrA name) {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		U32 count = 0;
		F32* data = read_full_file_to_arena<F32>(&count, arena, name);
		if (data) {
			using namespace VK;
			U32 width = 2048;
			U32 height = 1024;
			if (width * height * 4 * 4 >= imageCPUBuffer.capacity) {
				imageCPUBuffer.destroy();
				imageCPUBuffer.create(width * height * 4 * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK::hostMemoryTypeIndex);
			}
			{ // Create source image
				VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
				imgInfo.imageType = VK_IMAGE_TYPE_2D;
				imgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
				imgInfo.extent = VkExtent3D{ width, height, 1 };
				imgInfo.mipLevels = 1;
				imgInfo.arrayLayers = 1;
				imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				CHK_VK(vkCreateImage(logicalDevice, &imgInfo, nullptr, &equirectImage));

				VkMemoryRequirements memoryRequirements;
				vkGetImageMemoryRequirements(logicalDevice, equirectImage, &memoryRequirements);
				VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
				allocInfo.allocationSize = memoryRequirements.size;
				allocInfo.memoryTypeIndex = deviceMemoryTypeIndex;
				CHK_VK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &equirectImageMemory));
				CHK_VK(vkBindImageMemory(logicalDevice, equirectImage, equirectImageMemory, 0));

				VkImageViewCreateInfo imgViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				imgViewInfo.image = equirectImage;
				imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				imgViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
				imgViewInfo.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
				imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imgViewInfo.subresourceRange.baseMipLevel = 0;
				imgViewInfo.subresourceRange.levelCount = 1;
				imgViewInfo.subresourceRange.baseArrayLayer = 0;
				imgViewInfo.subresourceRange.layerCount = 1;
				CHK_VK(vkCreateImageView(logicalDevice, &imgViewInfo, nullptr, &equirectImageView));
			}

			VkWriteDescriptorSet write[2];
			write[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write[0].dstSet = cubemapConvolveDescriptorSet.descriptorSet;
			write[0].dstBinding = 0; // binding 0 is input
			write[0].descriptorCount = 1;
			write[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			VkDescriptorImageInfo equirectImageInfo;
			equirectImageInfo.sampler = VK_NULL_HANDLE;
			equirectImageInfo.imageView = equirectImageView;
			equirectImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			write[0].pImageInfo = &equirectImageInfo;
			write[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write[1].dstSet = cubemapConvolveDescriptorSet.descriptorSet;
			write[1].dstBinding = 1; // binding 1 is output
			write[1].descriptorCount = 1;
			write[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			VkDescriptorImageInfo cubeImageInfo;
			cubeImageInfo.sampler = VK_NULL_HANDLE;
			cubeImageInfo.imageView = cubeImgView;
			cubeImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			write[1].pImageInfo = &cubeImageInfo;
			vkUpdateDescriptorSets(logicalDevice, 2, write, 0, nullptr);

			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			CHK_VK(vkBeginCommandBuffer(convolveCmdBuf, &beginInfo));

			
			{ // Transition image layouts
				VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // General for compute shader loads
				barrier.srcQueueFamilyIndex = graphicsFamily;
				barrier.dstQueueFamilyIndex = graphicsFamily;
				barrier.image = equirectImage;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
				barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.image = cubeImg;
				barrier.subresourceRange.layerCount = 6;
				vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			}
			{ // Copy equirect to GPU
				memcpy(imageCPUBuffer.mapping, data, width * height * 4 * sizeof(F32));
				imageCPUBuffer.invalidate_mapped();
				VkBufferImageCopy copy{};
				copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copy.imageSubresource.mipLevel = 0;
				copy.imageSubresource.baseArrayLayer = 0;
				copy.imageSubresource.layerCount = 1;
				copy.imageExtent = VkExtent3D{ width, height, 1 };
				vkCmdCopyBufferToImage(convolveCmdBuf, imageCPUBuffer.buffer, equirectImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

				VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // General for compute shader loads
				barrier.srcQueueFamilyIndex = graphicsFamily;
				barrier.dstQueueFamilyIndex = graphicsFamily;
				barrier.image = equirectImage;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			}

			vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapConvolvePipeline);
			vkCmdBindDescriptorSets(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapConvolvePipelineLayout, 0, 1, &cubemapConvolveDescriptorSet.descriptorSet, 0, nullptr);
			CubemapConvolveInfo convolveComputeInfo{};
			convolveComputeInfo.inputDim = V2U{ width, height };
			convolveComputeInfo.outputDim = V2U{ CUBEMAP_RES, CUBEMAP_RES };
			vkCmdPushConstants(convolveCmdBuf, cubemapConvolvePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubemapConvolveInfo), &convolveComputeInfo);
			vkCmdDispatch(convolveCmdBuf, (width + 15) / 16, (height + 15) / 16, 6);
			
			{ // Get convolved image back into host memory
				VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.srcQueueFamilyIndex = graphicsFamily;
				barrier.dstQueueFamilyIndex = graphicsFamily;
				barrier.image = cubeImg;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				VkBufferImageCopy cubeRegion{};
				cubeRegion.bufferOffset = 0;
				// 0 means tightly packed
				cubeRegion.bufferRowLength = 0;
				cubeRegion.bufferImageHeight = 0;
				cubeRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				cubeRegion.imageSubresource.mipLevel = 0;
				cubeRegion.imageSubresource.baseArrayLayer = 0;
				cubeRegion.imageSubresource.layerCount = 1;
				cubeRegion.imageOffset = VkOffset3D{ 0, 0, 0 };
				cubeRegion.imageExtent = VkExtent3D{ CUBEMAP_RES, CUBEMAP_RES, 1 };
				vkCmdCopyImageToBuffer(convolveCmdBuf, cubeImg, VK_IMAGE_LAYOUT_GENERAL, imageCPUBuffer.buffer, 1, &cubeRegion);
			}

			CHK_VK(vkEndCommandBuffer(convolveCmdBuf));

			VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &convolveCmdBuf;
			CHK_VK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, convolveCompleteFence));
			// I should probably be doing this async, but whatever. A cubemap convolution will never happen during gameplay.
			CHK_VK(vkWaitForFences(logicalDevice, 1, &convolveCompleteFence, VK_TRUE, U64_MAX));
			CHK_VK(vkResetCommandPool(logicalDevice, convolveCommandPool, 0));

			{ // Destroy source image
				vkDestroyImageView(logicalDevice, equirectImageView, nullptr);
				vkDestroyImage(logicalDevice, equirectImage, nullptr);
				vkFreeMemory(logicalDevice, equirectImageMemory, nullptr);
			}
			write_data_to_file("test_convolve.dat"a, imageCPUBuffer.mapping, CUBEMAP_RES * CUBEMAP_RES * 4 * sizeof(F32));
		}
	}
}

}