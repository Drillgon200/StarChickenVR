#pragma once

#include "DrillLib.h"
#include "VK.h"
#include "ResourceLoading.h"
#include "PNG.h"

namespace CubemapGen {

VkCommandPool convolveCommandPool;
VkCommandBuffer convolveCmdBuf;

VkFence convolveCompleteFence;

VK::DedicatedBuffer imageCPUBuffer;

// The base cubemap, downsampled with the typical 2x2 box filter
VkImage cubeImg;
VkImageView cubeU32ImgViews[VK::MAX_MIP_LEVEL];
VkImageView cubeRGBImgView;
VkImageView cubeArrayRGBImgView;

// Convolved with the Trowbridge-Reitz BRDF
VkImage cubeTRConvImg;
VkImageView cubeTRConvU32ImgViews[VK::MAX_MIP_LEVEL];
VkImageView cubeTRConvRGBImgView;

// Convolved with a simple cosine hemisphere weight for lambert shading
// I couldn't find a good way to resolve Oren Nayar's view dependence, and it goes to lambert when view == normal anyway 
VkImage cubeCosConvImg;
VkImageView cubeCosConvU32ImgView;
VkImageView cubeCosConvRGBImgView;

VkImage trLUTImg;
VkImageView trLUTImgView;

VkDeviceMemory equirectImageMemory;
VkImage equirectImage;
VkImageView equirectImageView;

B32 hasCubemap;

const U32 CUBEMAP_SPECULAR_RES = 512;
const U32 CUBEMAP_DIFFUSE_RES = 64;
const U32 BRDF_LUT_RES = 512;

// At some point I'll refactor this code to support multiple cubemaps, but this is good enough for now
void init() {
	imageCPUBuffer.create(max<U32>(CUBEMAP_SPECULAR_RES * CUBEMAP_SPECULAR_RES * 4 * 6 * 2, 2048 * 2048 * 4 * 4), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK::hostMemoryTypeIndex);

	U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
	{ // Create cubemap images
		VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
		imgInfo.imageType = VK_IMAGE_TYPE_2D;
		imgInfo.format = VK_FORMAT_R32_UINT;
		imgInfo.extent = VkExtent3D{ CUBEMAP_SPECULAR_RES, CUBEMAP_SPECULAR_RES, 1 };
		imgInfo.mipLevels = cubeMipLevels;
		imgInfo.arrayLayers = 6;
		imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		CHK_VK(VK::vkCreateImage(VK::logicalDevice, &imgInfo, nullptr, &cubeImg));
		CHK_VK(VK::vkCreateImage(VK::logicalDevice, &imgInfo, nullptr, &cubeTRConvImg));
		imgInfo.extent = VkExtent3D{ CUBEMAP_DIFFUSE_RES, CUBEMAP_DIFFUSE_RES, 1 };
		imgInfo.mipLevels = 1;
		CHK_VK(VK::vkCreateImage(VK::logicalDevice, &imgInfo, nullptr, &cubeCosConvImg));
		imgInfo.flags = 0;
		imgInfo.arrayLayers = 1;
		imgInfo.extent = VkExtent3D{ BRDF_LUT_RES, BRDF_LUT_RES, 1 };
		imgInfo.format = VK_FORMAT_R8G8_UNORM;
		CHK_VK(VK::vkCreateImage(VK::logicalDevice, &imgInfo, nullptr, &trLUTImg));


		VkMemoryRequirements memoryRequirements;
		VK::vkGetImageMemoryRequirements(VK::logicalDevice, cubeImg, &memoryRequirements);
		VkDeviceMemory linearBlock;
		VkDeviceSize memoryOffset;
		ResourceLoading::alloc_texture_memory(&linearBlock, &memoryOffset, memoryRequirements);
		CHK_VK(VK::vkBindImageMemory(VK::logicalDevice, cubeImg, linearBlock, memoryOffset));

		VK::vkGetImageMemoryRequirements(VK::logicalDevice, cubeTRConvImg, &memoryRequirements);
		ResourceLoading::alloc_texture_memory(&linearBlock, &memoryOffset, memoryRequirements);
		CHK_VK(VK::vkBindImageMemory(VK::logicalDevice, cubeTRConvImg, linearBlock, memoryOffset));

		VK::vkGetImageMemoryRequirements(VK::logicalDevice, cubeCosConvImg, &memoryRequirements);
		ResourceLoading::alloc_texture_memory(&linearBlock, &memoryOffset, memoryRequirements);
		CHK_VK(VK::vkBindImageMemory(VK::logicalDevice, cubeCosConvImg, linearBlock, memoryOffset));

		VK::vkGetImageMemoryRequirements(VK::logicalDevice, trLUTImg, &memoryRequirements);
		ResourceLoading::alloc_texture_memory(&linearBlock, &memoryOffset, memoryRequirements);
		CHK_VK(VK::vkBindImageMemory(VK::logicalDevice, trLUTImg, linearBlock, memoryOffset));

		VkImageViewCreateInfo imgViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		imgViewInfo.format = VK_FORMAT_R32_UINT;
		imgViewInfo.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgViewInfo.subresourceRange.levelCount = 1;
		imgViewInfo.subresourceRange.baseArrayLayer = 0;
		imgViewInfo.subresourceRange.layerCount = 6;
		for (U32 i = 0; i < cubeMipLevels; i++) {
			imgViewInfo.subresourceRange.baseMipLevel = i;
			imgViewInfo.image = cubeImg;
			CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imgViewInfo, nullptr, &cubeU32ImgViews[i]));
			imgViewInfo.image = cubeTRConvImg;
			CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imgViewInfo, nullptr, &cubeTRConvU32ImgViews[i]));
		}
		imgViewInfo.subresourceRange.baseMipLevel = 0;
		imgViewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		imgViewInfo.image = cubeCosConvImg;
		CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imgViewInfo, nullptr, &cubeCosConvU32ImgView));
		imgViewInfo.format = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
		VkImageViewUsageCreateInfo rgbViewUsage{ VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
		rgbViewUsage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imgViewInfo.pNext = &rgbViewUsage;
		imgViewInfo.image = cubeImg;
		CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imgViewInfo, nullptr, &cubeRGBImgView));
		imgViewInfo.image = cubeTRConvImg;
		CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imgViewInfo, nullptr, &cubeTRConvRGBImgView));
		imgViewInfo.image = cubeCosConvImg;
		CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imgViewInfo, nullptr, &cubeCosConvRGBImgView));
		imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		imgViewInfo.image = cubeImg;
		CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imgViewInfo, nullptr, &cubeArrayRGBImgView));

		imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imgViewInfo.format = VK_FORMAT_R8G8_UNORM;
		imgViewInfo.subresourceRange.layerCount = 1;
		imgViewInfo.pNext = nullptr;
		imgViewInfo.image = trLUTImg;
		CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imgViewInfo, nullptr, &trLUTImgView));
	}
	{ // Write cubemap to descriptor set
		VkWriteDescriptorSet writes[3];
		VkWriteDescriptorSet& specularWrite = writes[0];
		specularWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		specularWrite.dstSet = VK::drawDataDescriptorSet.descriptorSet;
		specularWrite.dstBinding = 2; // Specular cubemap
		specularWrite.dstArrayElement = 0;
		specularWrite.descriptorCount = 1;
		specularWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		VkDescriptorImageInfo specularImageInfo;
		specularImageInfo.sampler = VK_NULL_HANDLE;
		specularImageInfo.imageView = cubeTRConvRGBImgView;
		specularImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		specularWrite.pImageInfo = &specularImageInfo;
		VkWriteDescriptorSet& diffuseWrite = writes[1];
		diffuseWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		diffuseWrite.dstSet = VK::drawDataDescriptorSet.descriptorSet;
		diffuseWrite.dstBinding = 3; // Diffuse cubemap
		diffuseWrite.dstArrayElement = 0;
		diffuseWrite.descriptorCount = 1;
		diffuseWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		VkDescriptorImageInfo diffuseImageInfo;
		diffuseImageInfo.sampler = VK_NULL_HANDLE;
		diffuseImageInfo.imageView = cubeCosConvRGBImgView;
		diffuseImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		diffuseWrite.pImageInfo = &diffuseImageInfo;
		VkWriteDescriptorSet& lutWrite = writes[2];
		lutWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		lutWrite.dstSet = VK::drawDataDescriptorSet.descriptorSet;
		lutWrite.dstBinding = 4; // TR LUT
		lutWrite.dstArrayElement = 0;
		lutWrite.descriptorCount = 1;
		lutWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		VkDescriptorImageInfo lutImageInfo;
		lutImageInfo.sampler = VK_NULL_HANDLE;
		lutImageInfo.imageView = trLUTImgView;
		lutImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		lutWrite.pImageInfo = &lutImageInfo;
		VK::vkUpdateDescriptorSets(VK::logicalDevice, 3, writes, 0, nullptr);
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
	U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
	for (U32 i = 0; i < cubeMipLevels; i++) {
		VK::vkDestroyImageView(VK::logicalDevice, cubeU32ImgViews[i], nullptr);
		VK::vkDestroyImageView(VK::logicalDevice, cubeTRConvU32ImgViews[i], nullptr);
	}
	VK::vkDestroyImageView(VK::logicalDevice, cubeCosConvU32ImgView, nullptr);
	VK::vkDestroyImageView(VK::logicalDevice, cubeRGBImgView, nullptr);
	VK::vkDestroyImageView(VK::logicalDevice, cubeTRConvRGBImgView, nullptr);
	VK::vkDestroyImageView(VK::logicalDevice, cubeArrayRGBImgView, nullptr);
	VK::vkDestroyImageView(VK::logicalDevice, cubeCosConvRGBImgView, nullptr);
	VK::vkDestroyImageView(VK::logicalDevice, trLUTImgView, nullptr);
	VK::vkDestroyImage(VK::logicalDevice, cubeImg, nullptr);
	VK::vkDestroyImage(VK::logicalDevice, cubeTRConvImg, nullptr);
	VK::vkDestroyImage(VK::logicalDevice, cubeCosConvImg, nullptr);
	VK::vkDestroyImage(VK::logicalDevice, trLUTImg, nullptr);
	imageCPUBuffer.destroy();
}

void equirectangular2convolved_cubemap(StrA name) {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		U32 count = 0;
		F32* data = read_full_file_to_arena<F32>(&count, arena, name);
		if (data) {
			using namespace VK;
			U32 width = 1024;
			U32 height = 512;
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
				imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

			{ // Update descriptor set
				VkWriteDescriptorSet write[5 + MAX_MIP_LEVEL * 3];
				U32 writeCount = 0;
				VkWriteDescriptorSet& samplerWrite = write[writeCount++];
				samplerWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				samplerWrite.dstSet = cubemapConvolveDescriptorSet.descriptorSet;
				samplerWrite.dstBinding = 0; // binding 0 is bilinear sampler
				samplerWrite.descriptorCount = 1;
				samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				VkDescriptorImageInfo samplerInfo{};
				samplerInfo.sampler = linearSampler;
				samplerWrite.pImageInfo = &samplerInfo;

				VkWriteDescriptorSet& equirectWrite = write[writeCount++];
				equirectWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				equirectWrite.dstSet = cubemapConvolveDescriptorSet.descriptorSet;
				equirectWrite.dstBinding = 3; // binding 3 is equirect input
				equirectWrite.descriptorCount = 1;
				equirectWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				VkDescriptorImageInfo equirectImageInfo;
				equirectImageInfo.sampler = VK_NULL_HANDLE;
				equirectImageInfo.imageView = equirectImageView;
				equirectImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				equirectWrite.pImageInfo = &equirectImageInfo;

				VkWriteDescriptorSet& cubeInputWrite = write[writeCount++];
				cubeInputWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				cubeInputWrite.dstSet = cubemapConvolveDescriptorSet.descriptorSet;
				cubeInputWrite.dstBinding = 2; // binding 2 is cube input
				cubeInputWrite.descriptorCount = 1;
				cubeInputWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				VkDescriptorImageInfo cubeImageInfo{};
				cubeImageInfo.imageView = cubeRGBImgView;
				cubeImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				cubeInputWrite.pImageInfo = &cubeImageInfo;

				VkWriteDescriptorSet& arrayLayersInputWrite = write[writeCount++];
				arrayLayersInputWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				arrayLayersInputWrite.dstSet = cubemapConvolveDescriptorSet.descriptorSet;
				arrayLayersInputWrite.dstBinding = 4; // binding 4 is cube array layers input
				arrayLayersInputWrite.descriptorCount = 1;
				arrayLayersInputWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				VkDescriptorImageInfo arrayLayersImageInfo{};
				arrayLayersImageInfo.imageView = cubeArrayRGBImgView;
				arrayLayersImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				arrayLayersInputWrite.pImageInfo = &arrayLayersImageInfo;
				
				U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
				VkDescriptorImageInfo cubeMipInfo[MAX_MIP_LEVEL]{};
				VkDescriptorImageInfo cubeTRConvMipInfo[MAX_MIP_LEVEL]{};
				VkDescriptorImageInfo cubeEONConvMipInfo[MAX_MIP_LEVEL]{};
				for (U32 i = 0; i < cubeMipLevels; i++) {
					VkWriteDescriptorSet& cubeMipWrite = write[writeCount++];
					cubeMipWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					cubeMipWrite.dstSet = cubemapConvolveDescriptorSet.descriptorSet;
					cubeMipWrite.dstBinding = 1; // binding 1 is mip output
					cubeMipWrite.dstArrayElement = i;
					cubeMipWrite.descriptorCount = 1;
					cubeMipWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					
					cubeMipInfo[i].imageView = cubeU32ImgViews[i];
					cubeMipInfo[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
					cubeMipWrite.pImageInfo = &cubeMipInfo[i];

					VkWriteDescriptorSet& cubeTRConvMipWrite = write[writeCount++];
					cubeTRConvMipWrite = cubeMipWrite;
					cubeTRConvMipWrite.dstBinding = 5; // binding 5 is convolution output
					cubeTRConvMipInfo[i].imageView = cubeTRConvU32ImgViews[i];
					cubeTRConvMipInfo[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
					cubeTRConvMipWrite.pImageInfo = &cubeTRConvMipInfo[i];

					VkWriteDescriptorSet& cubeEONConvMipWrite = write[writeCount++];
					cubeEONConvMipWrite = cubeMipWrite;
					cubeEONConvMipWrite.dstBinding = 6; // binding 6 is EON convolution output
					cubeEONConvMipInfo[i].imageView = cubeCosConvU32ImgView;
					cubeEONConvMipInfo[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
					cubeEONConvMipWrite.pImageInfo = &cubeEONConvMipInfo[i];
				}

				VkWriteDescriptorSet& trLutWrite = write[writeCount++];
				trLutWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				trLutWrite.dstSet = cubemapConvolveDescriptorSet.descriptorSet;
				trLutWrite.dstBinding = 7; // binding 7 is tr LUT output
				trLutWrite.descriptorCount = 1;
				trLutWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				VkDescriptorImageInfo trLutImageInfo{};
				trLutImageInfo.imageView = trLUTImgView;
				trLutImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				trLutWrite.pImageInfo = &trLutImageInfo;

				vkUpdateDescriptorSets(logicalDevice, writeCount, write, 0, nullptr);
			}
			
			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			CHK_VK(vkBeginCommandBuffer(convolveCmdBuf, &beginInfo));

			
			{ // Transition image layouts
				VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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
				barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
				barrier.subresourceRange.layerCount = 6;
				vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				barrier.image = cubeTRConvImg;
				vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				barrier.image = cubeCosConvImg;
				vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				barrier.subresourceRange.layerCount = 1;
				barrier.image = trLUTImg;
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
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

			vkCmdBindDescriptorSets(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapComputePipelineLayout, 0, 1, &cubemapConvolveDescriptorSet.descriptorSet, 0, nullptr);
			{ // Copy base from equirect
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapFromEquirectPipeline);
				CubemapPipelineInfo copyEquirectInfo{};
				copyEquirectInfo.inputDim = V2U{ width, height };
				copyEquirectInfo.outputDim = V2U{ CUBEMAP_SPECULAR_RES, CUBEMAP_SPECULAR_RES };
				vkCmdPushConstants(convolveCmdBuf, cubemapComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubemapPipelineInfo), &copyEquirectInfo);
				vkCmdDispatch(convolveCmdBuf, (CUBEMAP_SPECULAR_RES + 15) / 16, (CUBEMAP_SPECULAR_RES + 15) / 16, 6);
			}
			{ // Generate mips
				U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapMipgenPipeline);
				for (U32 i = 1; i < cubeMipLevels; i++) {
					VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
					barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
					barrier.srcQueueFamilyIndex = graphicsFamily;
					barrier.dstQueueFamilyIndex = graphicsFamily;
					barrier.image = cubeImg;
					barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					barrier.subresourceRange.baseMipLevel = i - 1;
					barrier.subresourceRange.levelCount = 1;
					barrier.subresourceRange.baseArrayLayer = 0;
					barrier.subresourceRange.layerCount = 1;
					vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

					U32 res = max((CUBEMAP_SPECULAR_RES >> i), 1u);
					CubemapPipelineInfo mipgenInfo{};
					mipgenInfo.outputDim = V2U{ res, res };
					mipgenInfo.inputIdx = i - 1;
					mipgenInfo.outputIdx = i;
					vkCmdPushConstants(convolveCmdBuf, cubemapComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubemapPipelineInfo), &mipgenInfo);
					vkCmdDispatch(convolveCmdBuf, (res + 15) / 16, (res + 15) / 16, 6);
				}
			}
			{ // Generate TR BRDF LUT
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, brdfTRLutPipeline);
				CubemapPipelineInfo lutInfo{};
				lutInfo.outputDim = V2U{ BRDF_LUT_RES, BRDF_LUT_RES };
				vkCmdPushConstants(convolveCmdBuf, cubemapComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubemapPipelineInfo), &lutInfo);
				vkCmdDispatch(convolveCmdBuf, (lutInfo.outputDim.x + 15) / 16, (lutInfo.outputDim.y + 15) / 16, 1);
			}
			{ // Do convolution from mips to final map
				U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapTRConvolvePipeline);
				for (U32 i = 0; i < cubeMipLevels; i++) {
					U32 res = max((CUBEMAP_SPECULAR_RES >> i), 1u);
					CubemapPipelineInfo convolveInfo{};
					convolveInfo.inputDim = V2U{ CUBEMAP_SPECULAR_RES, CUBEMAP_SPECULAR_RES };
					convolveInfo.outputDim = V2U{ res, res };
					convolveInfo.roughness = max(0.01F, F32(i) / F32(cubeMipLevels - 1));
					convolveInfo.outputIdx = i;
					vkCmdPushConstants(convolveCmdBuf, cubemapComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubemapPipelineInfo), &convolveInfo);
					vkCmdDispatch(convolveCmdBuf, (res + 15) / 16, (res + 15) / 16, 6);
				}
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapCosConvolvePipeline);
				CubemapPipelineInfo convolveInfo{};
				convolveInfo.inputDim = V2U{ CUBEMAP_SPECULAR_RES, CUBEMAP_SPECULAR_RES };
				convolveInfo.outputDim = V2U{ CUBEMAP_DIFFUSE_RES, CUBEMAP_DIFFUSE_RES };
				convolveInfo.roughness = 1.0F;
				convolveInfo.outputIdx = 0;
				vkCmdPushConstants(convolveCmdBuf, cubemapComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubemapPipelineInfo), &convolveInfo);
				vkCmdDispatch(convolveCmdBuf, (convolveInfo.outputDim.x + 15) / 16, (convolveInfo.outputDim.y + 15) / 16, 6);
			}
			
			
			U32 cubemapTotalSize = 0;
			{ // Get convolved image back into host memory
				VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.srcQueueFamilyIndex = graphicsFamily;
				barrier.dstQueueFamilyIndex = graphicsFamily;
				barrier.image = cubeTRConvImg;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 6;
				vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				barrier.image = cubeCosConvImg;
				vkCmdPipelineBarrier(convolveCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
				for (U32 i = 0; i < cubeMipLevels; i++) {
					VkBufferImageCopy cubeRegion{};
					cubeRegion.bufferOffset = cubemapTotalSize;
					// 0 means tightly packed
					cubeRegion.bufferRowLength = 0;
					cubeRegion.bufferImageHeight = 0;
					cubeRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					cubeRegion.imageSubresource.mipLevel = i;
					cubeRegion.imageSubresource.baseArrayLayer = 0;
					cubeRegion.imageSubresource.layerCount = 6;
					cubeRegion.imageOffset = VkOffset3D{ 0, 0, 0 };
					U32 res = max((CUBEMAP_SPECULAR_RES >> i), 1u);
					cubeRegion.imageExtent = VkExtent3D{ res, res, 1 };
					vkCmdCopyImageToBuffer(convolveCmdBuf, cubeTRConvImg, VK_IMAGE_LAYOUT_GENERAL, imageCPUBuffer.buffer, 1, &cubeRegion);
					cubemapTotalSize += res * res * sizeof(U32) * 6;
				}
				VkBufferImageCopy cubeRegion{};
				cubeRegion.bufferOffset = cubemapTotalSize;
				// 0 means tightly packed
				cubeRegion.bufferRowLength = 0;
				cubeRegion.bufferImageHeight = 0;
				cubeRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				cubeRegion.imageSubresource.mipLevel = 0;
				cubeRegion.imageSubresource.baseArrayLayer = 0;
				cubeRegion.imageSubresource.layerCount = 6;
				cubeRegion.imageOffset = VkOffset3D{ 0, 0, 0 };
				cubeRegion.imageExtent = VkExtent3D{ CUBEMAP_DIFFUSE_RES, CUBEMAP_DIFFUSE_RES, 1 };
				vkCmdCopyImageToBuffer(convolveCmdBuf, cubeCosConvImg, VK_IMAGE_LAYOUT_GENERAL, imageCPUBuffer.buffer, 1, &cubeRegion);
			}

			CHK_VK(vkEndCommandBuffer(convolveCmdBuf));

			VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &convolveCmdBuf;
			CHK_VK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, convolveCompleteFence));
			F64 t0 = current_time_seconds();

			// I should probably be doing this async, but whatever. A cubemap convolution will never happen during gameplay.
			CHK_VK(vkWaitForFences(logicalDevice, 1, &convolveCompleteFence, VK_TRUE, U64_MAX));
			CHK_VK(vkResetFences(logicalDevice, 1, &convolveCompleteFence));
			CHK_VK(vkResetCommandPool(logicalDevice, convolveCommandPool, 0));
			printf("Time taken: %\n"a, current_time_seconds() - t0);

			{ // Destroy source image
				vkDestroyImageView(logicalDevice, equirectImageView, nullptr);
				vkDestroyImage(logicalDevice, equirectImage, nullptr);
				vkFreeMemory(logicalDevice, equirectImageMemory, nullptr);
			}
			write_data_to_file("cubemap_test/convolved.dat"a, imageCPUBuffer.mapping, cubemapTotalSize);
			write_data_to_file("cubemap_test/convolved_diffuse.dat"a, (char*)imageCPUBuffer.mapping + cubemapTotalSize, CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES * 6 * sizeof(U32));
			
			RGBA8* colors = arena.alloc<RGBA8>(CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES);
			for (U32 layer = 0; layer < 6; layer++) {
				for (U32 i = 0; i < CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES; i++) {
					U32 packed = ((U32*)((char*)imageCPUBuffer.mapping + cubemapTotalSize + layer * CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES * sizeof(U32)))[i];
					colors[i] = tonemap_E5B9G9R9(packed);
				}
				PNG::write_image(strafmt(arena, "cubemap_test/diffuse_%.png"a, layer), colors, CUBEMAP_DIFFUSE_RES, CUBEMAP_DIFFUSE_RES);
			}
			hasCubemap = true;
		}
	}
}

}