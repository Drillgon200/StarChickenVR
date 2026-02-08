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
U32 cubeU32ImgDescriptors[VK::MAX_MIP_LEVEL];
U32 cubeRGBImgDescriptor;
U32 cubeArrayRGBImgDescriptor;

// Convolved with the Trowbridge-Reitz BRDF
VkImage cubeTRConvImg;
U32 cubeTRConvU32ImgDescriptors[VK::MAX_MIP_LEVEL];
U32 cubeTRConvRGBImgDescriptor;

// Convolved with a simple cosine hemisphere weight for lambert shading
// I couldn't find a good way to resolve Oren Nayar's view dependence, and it goes to lambert when view == normal anyway 
VkImage cubeCosConvImg;
U32 cubeCosConvU32ImgDescriptor;
U32 cubeCosConvRGBImgDescriptor;

VkImage trLUTImg;
U32 trLUTImgDescriptor;

VkDeviceMemory equirectImageMemory;
VkImage equirectImage;
U32 equirectImageDescriptor;

#pragma pack(push, 1)
struct CubemapFromEquirectPushData {
	V2U outputDimensions;
	U32 inputImgEquirect;
	U32 outputImgCube;
};
struct CubeMipGenPushData {
	V2U outputDimensions;
	U32 inputCube;
	U32 inputMipLevel;
	U32 outputCube;
};
struct BRDFLUTPushData {
	V2U outputDimensions;
	U32 outputTRLut;
};
struct CosConvolvePushData {
	V2U inputDimensions;
	V2U outputDimensions;
	U32 inputImgCube;
	U32 outputImg;
};
struct TRConvolvePushData {
	V2U inputDimensions;
	V2U outputDimensions;
	U32 inputImg;
	U32 outputImg;
	F32 roughness;
};
#pragma pack(pop)

const U32 CUBEMAP_SPECULAR_RES = 512;
const U32 CUBEMAP_DIFFUSE_RES = 64;
const U32 BRDF_LUT_RES = 512;

void init() {
	imageCPUBuffer.create(max<U32>(CUBEMAP_SPECULAR_RES * CUBEMAP_SPECULAR_RES * 4 * 6 * 2 + BRDF_LUT_RES * BRDF_LUT_RES * 4, 2048 * 2048 * 4 * 4), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK::hostCachedMemoryTypeIndex);

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

		ResourceLoading::alloc_and_bind_texture_memory(cubeImg);
		ResourceLoading::alloc_and_bind_texture_memory(cubeTRConvImg);
		ResourceLoading::alloc_and_bind_texture_memory(cubeCosConvImg);
		ResourceLoading::alloc_and_bind_texture_memory(trLUTImg);

		for (U32 i = 0; i < cubeMipLevels; i++) {
			cubeU32ImgDescriptors[i] = VK::descriptorHeap.make_image(cubeImg, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R32_UINT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 6, 0);
			cubeTRConvU32ImgDescriptors[i] = VK::descriptorHeap.make_image(cubeTRConvImg, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R32_UINT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 6, 0);
		}
		cubeCosConvU32ImgDescriptor = VK::descriptorHeap.make_image(cubeCosConvImg, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R32_UINT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6, 0);
		cubeRGBImgDescriptor = VK::descriptorHeap.make_image(cubeImg, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 6, VK_IMAGE_USAGE_SAMPLED_BIT);
		cubeTRConvRGBImgDescriptor = VK::descriptorHeap.make_image(cubeTRConvImg, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 6, VK_IMAGE_USAGE_SAMPLED_BIT);
		cubeCosConvRGBImgDescriptor = VK::descriptorHeap.make_image(cubeCosConvImg, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 6, VK_IMAGE_USAGE_SAMPLED_BIT);
		cubeArrayRGBImgDescriptor = VK::descriptorHeap.make_image(cubeImg, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 6, VK_IMAGE_USAGE_SAMPLED_BIT);
		trLUTImgDescriptor = VK::descriptorHeap.make_storage_image(trLUTImg, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8_UNORM);
	}

	CHK_VK(VK::vkCreateCommandPool(VK::logicalDevice, ptr(VkCommandPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = VK::graphicsFamily }), nullptr, &convolveCommandPool));
	VkCommandBufferAllocateInfo cmdBufInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = convolveCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	CHK_VK(VK::vkAllocateCommandBuffers(VK::logicalDevice, &cmdBufInfo, &convolveCmdBuf));

	VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	CHK_VK(VK::vkCreateFence(VK::logicalDevice, &fenceInfo, nullptr, &convolveCompleteFence));
}

void destroy() {
	VK::vkDestroyFence(VK::logicalDevice, convolveCompleteFence, nullptr);
	VK::vkDestroyCommandPool(VK::logicalDevice, convolveCommandPool, nullptr);
	VK::vkDestroyImage(VK::logicalDevice, cubeImg, nullptr);
	VK::vkDestroyImage(VK::logicalDevice, cubeTRConvImg, nullptr);
	VK::vkDestroyImage(VK::logicalDevice, cubeCosConvImg, nullptr);
	VK::vkDestroyImage(VK::logicalDevice, trLUTImg, nullptr);
	imageCPUBuffer.destroy();
}

void equirectangular2convolved_cubemap(StrA dstPath, StrA srcEquirect, bool writeTRLut) {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		U32 count = 0;
		F32* data = read_full_file_to_arena<F32>(&count, arena, srcEquirect);
		if (data) {
			using namespace VK;
			U32 width = 1024;
			U32 height = 512;
			if (width * height * 4 * 4 >= imageCPUBuffer.capacity) {
				imageCPUBuffer.destroy();
				imageCPUBuffer.create(width * height * 4 * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK::hostCachedMemoryTypeIndex);
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

				equirectImageDescriptor = descriptorHeap.make_sampled_image(equirectImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT);
			}

			CHK_VK(vkBeginCommandBuffer(convolveCmdBuf, ptr(VkCommandBufferBeginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT })));
			
			img_init_barrier(convolveCmdBuf, equirectImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
			img_init_barrier(convolveCmdBuf, cubeImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
			img_init_barrier(convolveCmdBuf, cubeTRConvImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
			img_init_barrier(convolveCmdBuf, cubeCosConvImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
			img_init_barrier(convolveCmdBuf, trLUTImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);

			{ // Copy equirect to GPU
				memcpy(imageCPUBuffer.mapping, data, width * height * 4 * sizeof(F32));
				imageCPUBuffer.invalidate_mapped();
				VkBufferImageCopy copy{
					.imageSubresource = VkImageSubresourceLayers{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = 0,
						.baseArrayLayer = 0,
						.layerCount = 1
					},
					.imageExtent = VkExtent3D{ width, height, 1 }
				};
				vkCmdCopyBufferToImage(convolveCmdBuf, imageCPUBuffer.buffer, equirectImage, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
				img_barrier(convolveCmdBuf, equirectImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			}

			{ // Copy base from equirect
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapFromEquirectPipeline);
				CubemapFromEquirectPushData fromEquirectPushData{
					.outputDimensions = V2U{ CUBEMAP_SPECULAR_RES, CUBEMAP_SPECULAR_RES },
					.inputImgEquirect = equirectImageDescriptor,
					.outputImgCube = cubeU32ImgDescriptors[0]
				};
				VK_PUSH_STRUCT(convolveCmdBuf, fromEquirectPushData, 0);
				vkCmdDispatch(convolveCmdBuf, (CUBEMAP_SPECULAR_RES + 15) / 16, (CUBEMAP_SPECULAR_RES + 15) / 16, 6);
			}
			{ // Generate mips
				U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapMipgenPipeline);
				for (U32 i = 1; i < cubeMipLevels; i++) {
					img_barrier(convolveCmdBuf, cubeImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

					U32 res = max((CUBEMAP_SPECULAR_RES >> i), 1u);
					CubeMipGenPushData mipGenPushData{
						.outputDimensions = V2U{ res, res },
						.inputCube = cubeArrayRGBImgDescriptor,
						.inputMipLevel = i - 1,
						.outputCube = cubeU32ImgDescriptors[i]
					};
					VK_PUSH_STRUCT(convolveCmdBuf, mipGenPushData, 0);
					vkCmdDispatch(convolveCmdBuf, (res + 15) / 16, (res + 15) / 16, 6);
				}
			}
			{ // Generate TR BRDF LUT
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, brdfTRLutPipeline);
				BRDFLUTPushData trLutPushData{ .outputDimensions = V2U{ BRDF_LUT_RES, BRDF_LUT_RES }, .outputTRLut = trLUTImgDescriptor };
				VK_PUSH_STRUCT(convolveCmdBuf, trLutPushData, 0);
				vkCmdDispatch(convolveCmdBuf, (BRDF_LUT_RES + 15) / 16, (BRDF_LUT_RES + 15) / 16, 1);
			}
			{ // Do convolution from mips to final map
				U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapTRConvolvePipeline);
				for (U32 i = 0; i < cubeMipLevels; i++) {
					U32 res = max((CUBEMAP_SPECULAR_RES >> i), 1u);
					TRConvolvePushData trConvolvePushData{
						.inputDimensions = V2U{ CUBEMAP_SPECULAR_RES, CUBEMAP_SPECULAR_RES },
						.outputDimensions = V2U{ res, res },
						.inputImg = cubeRGBImgDescriptor,
						.outputImg = cubeTRConvU32ImgDescriptors[i],
						.roughness = max(0.01F, F32(i) / F32(cubeMipLevels - 1))
					};
					VK_PUSH_STRUCT(convolveCmdBuf, trConvolvePushData, 0);
					vkCmdDispatch(convolveCmdBuf, (res + 15) / 16, (res + 15) / 16, 6);
				}
				vkCmdBindPipeline(convolveCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapCosConvolvePipeline);
				CosConvolvePushData cosConvolvePushData{
					.inputDimensions = V2U{ CUBEMAP_SPECULAR_RES, CUBEMAP_SPECULAR_RES },
					.outputDimensions = V2U{ CUBEMAP_DIFFUSE_RES, CUBEMAP_DIFFUSE_RES },
					.inputImgCube = cubeRGBImgDescriptor,
					.outputImg = cubeCosConvU32ImgDescriptor
				};
				VK_PUSH_STRUCT(convolveCmdBuf, cosConvolvePushData, 0);
				vkCmdDispatch(convolveCmdBuf, (CUBEMAP_DIFFUSE_RES + 15) / 16, (CUBEMAP_DIFFUSE_RES + 15) / 16, 6);
			}
			
			
			U32 cubemapSpecularTotalSize = 0;
			{ // Get convolved image back into host memory
				VK::img_barrier(convolveCmdBuf, cubeTRConvImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
				VK::img_barrier(convolveCmdBuf, cubeCosConvImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
				VK::img_barrier(convolveCmdBuf, trLUTImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

				U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
				for (U32 i = 0; i < cubeMipLevels; i++) {
					VkBufferImageCopy cubeRegion{};
					cubeRegion.bufferOffset = cubemapSpecularTotalSize;
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
					cubemapSpecularTotalSize += res * res * sizeof(U32) * 6;
				}
				VkBufferImageCopy cubeRegion{};
				cubeRegion.bufferOffset = cubemapSpecularTotalSize;
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
				VkBufferImageCopy trLUTRegion{};
				trLUTRegion.bufferOffset = cubemapSpecularTotalSize + CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES * 6 * sizeof(U32);
				// 0 means tightly packed
				trLUTRegion.bufferRowLength = 0;
				trLUTRegion.bufferImageHeight = 0;
				trLUTRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				trLUTRegion.imageSubresource.mipLevel = 0;
				trLUTRegion.imageSubresource.baseArrayLayer = 0;
				trLUTRegion.imageSubresource.layerCount = 1;
				trLUTRegion.imageOffset = VkOffset3D{ 0, 0, 0 };
				trLUTRegion.imageExtent = VkExtent3D{ BRDF_LUT_RES, BRDF_LUT_RES, 1 };
				vkCmdCopyImageToBuffer(convolveCmdBuf, trLUTImg, VK_IMAGE_LAYOUT_GENERAL, imageCPUBuffer.buffer, 1, &trLUTRegion);
				VK::buffer_barrier(convolveCmdBuf, imageCPUBuffer.buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
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
				descriptorHeap.free_slot(equirectImageDescriptor);
				vkDestroyImage(logicalDevice, equirectImage, nullptr);
				vkFreeMemory(logicalDevice, equirectImageMemory, nullptr);
			}
			if (File file = open_file_for_writing(stracat(arena, dstPath, "_specular.dtf"a))) {
				U32 cubeMipLevels = log2floor32(CUBEMAP_SPECULAR_RES) + 1;
				ResourceLoading::DTFHeader header{};
				memcpy(header.magic, "DUCK", 4);
				header.version = DRILL_LIB_MAKE_VERSION(2, 0, 0);
				header.flags = ResourceLoading::TEXTURE_FLAG_CUBE;
				header.format = ResourceLoading::TEXTURE_FORMAT_R9G9B9E5;
				header.mipCount = cubeMipLevels;
				header.width = CUBEMAP_SPECULAR_RES;
				header.height = CUBEMAP_SPECULAR_RES;
				write_file(file, &header, sizeof(header));
				void* specularData = imageCPUBuffer.mapping;
				write_file(file, specularData, cubemapSpecularTotalSize);
				close_file(file);
			}
			if (File file = open_file_for_writing(stracat(arena, dstPath, "_diffuse.dtf"a))) {
				ResourceLoading::DTFHeader header{};
				memcpy(header.magic, "DUCK", 4);
				header.version = DRILL_LIB_MAKE_VERSION(2, 0, 0);
				header.flags = ResourceLoading::TEXTURE_FLAG_CUBE;
				header.format = ResourceLoading::TEXTURE_FORMAT_R9G9B9E5;
				header.mipCount = 1;
				header.width = CUBEMAP_DIFFUSE_RES;
				header.height = CUBEMAP_DIFFUSE_RES;
				write_file(file, &header, sizeof(header));
				void* specularData = (char*)imageCPUBuffer.mapping + cubemapSpecularTotalSize;
				write_file(file, specularData, CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES * 6 * sizeof(U32));
				close_file(file);
			}
			if (writeTRLut) {
				// The TR LUT is kind of hacked in here because I only have to do it once, and I'm just keeping the code around in case I have to debug or change it
				if (File file = open_file_for_writing(stracat(arena, dstPath, "_trowbridge_reitz_lut.dtf"a))) {
					ResourceLoading::DTFHeader header{};
					memcpy(header.magic, "DUCK", 4);
					header.version = DRILL_LIB_MAKE_VERSION(2, 0, 0);
					header.flags = 0;
					header.format = ResourceLoading::TEXTURE_FORMAT_RG_U8;
					header.mipCount = 1;
					header.width = BRDF_LUT_RES;
					header.height = BRDF_LUT_RES;
					write_file(file, &header, sizeof(header));
					void* specularData = (char*)imageCPUBuffer.mapping + cubemapSpecularTotalSize + CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES * 6 * sizeof(U32);
					write_file(file, specularData, BRDF_LUT_RES * BRDF_LUT_RES * sizeof(RG8));
					close_file(file);
				}
			}

			/*RGBA8* colors = arena.alloc<RGBA8>(CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES);
			for (U32 layer = 0; layer < 6; layer++) {
				for (U32 i = 0; i < CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES; i++) {
					U32 packed = ((U32*)((char*)imageCPUBuffer.mapping + cubemapSpecularTotalSize + layer * CUBEMAP_DIFFUSE_RES * CUBEMAP_DIFFUSE_RES * sizeof(U32)))[i];
					colors[i] = tonemap_E5B9G9R9(packed);
				}
				PNG::write_image(strafmt(arena, "cubemap_test/diffuse_%.png"a, layer), colors, CUBEMAP_DIFFUSE_RES, CUBEMAP_DIFFUSE_RES);
			}*/
		}
	}
}

}