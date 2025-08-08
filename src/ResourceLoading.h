#pragma once
#include "VKGeometry_decl.h"
#include "VK_decl.h"
#include "PNG.h"

namespace ResourceLoading {

enum DMFObjectID : U32 {
	DMF_OBJECT_ID_NONE = 0,
	DMF_OBJECT_ID_MESH = 1,
	DMF_OBJECT_ID_ANIMATED_MESH = 2,
	DMF_OBJECT_ID_Count = 3
};

static constexpr U32 LAST_KNOWN_DMF_VERSION = DRILL_LIB_MAKE_VERSION(3, 0, 0);
static constexpr U32 LAST_KNOWN_DAF_VERSION = DRILL_LIB_MAKE_VERSION(2, 0, 0);

void read_and_upload_dmf_mesh(VKGeometry::StaticMesh* mesh, ByteBuf& modelFile, U32* skinningDataOffsetOut) {
	U32 numVertices = modelFile.read_u32();
	U32 numIndices = modelFile.read_u32();
	U32 vertexDataSize = numVertices * VK::VERTEX_FORMAT_POS3F_TEX2F_NORM3F_TAN3F_SIZE;
	if (skinningDataOffsetOut != nullptr) {
		vertexDataSize += numVertices * VK::VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE;
	}
	U32 indexDataSize = numIndices * sizeof(U16);
	if (modelFile.failed) {
		print("Model failed read position: ");
		println_integer(modelFile.offset);
		abort("Model format incorrect");
	}
	if (!modelFile.has_data_left(vertexDataSize + indexDataSize)) {
		abort("Model file does not have enough data for all vertices and indices");
	}
	mesh->indicesCount = numIndices;
	mesh->verticesCount = numVertices;
	if (skinningDataOffsetOut) {
		VK::geometryHandler.alloc_skeletal(&mesh->indicesOffset, &mesh->verticesOffset, skinningDataOffsetOut, numIndices, numVertices);
	} else {
		VK::geometryHandler.alloc_static(&mesh->indicesOffset, &mesh->verticesOffset, numIndices, numVertices);
	}
	VK::graphicsStager.upload_to_buffer(VK::geometryHandler.buffer, VK::geometryHandler.positionsOffset + mesh->verticesOffset * sizeof(V3F32), modelFile.bytes + modelFile.offset, numVertices * sizeof(V3F32));
	modelFile.offset += numVertices * sizeof(V3F32);
	VK::graphicsStager.upload_to_buffer(VK::geometryHandler.buffer, VK::geometryHandler.texcoordsOffset + mesh->verticesOffset * sizeof(V2F32), modelFile.bytes + modelFile.offset, numVertices * sizeof(V2F32));
	modelFile.offset += numVertices * sizeof(V2F32);
	VK::graphicsStager.upload_to_buffer(VK::geometryHandler.buffer, VK::geometryHandler.normalsOffset + mesh->verticesOffset * sizeof(V3F32), modelFile.bytes + modelFile.offset, numVertices * sizeof(V3F32));
	modelFile.offset += numVertices * sizeof(V3F32);
	VK::graphicsStager.upload_to_buffer(VK::geometryHandler.buffer, VK::geometryHandler.tangentsOffset + mesh->verticesOffset * sizeof(V3F32), modelFile.bytes + modelFile.offset, numVertices * sizeof(V3F32));
	modelFile.offset += numVertices * sizeof(V3F32);
	if (skinningDataOffsetOut) {
		VK::graphicsStager.upload_to_buffer(VK::geometryHandler.buffer, VK::geometryHandler.skinDataOffset + *skinningDataOffsetOut * U64(VK::VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE), modelFile.bytes + modelFile.offset, numVertices * U64(VK::VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE));
		modelFile.offset += numVertices * VK::VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE;
	}
	VK::graphicsStager.upload_to_buffer(VK::geometryHandler.buffer, VK::geometryHandler.indicesOffset + mesh->indicesOffset * sizeof(U16), modelFile.bytes + modelFile.offset, numIndices * sizeof(U16));
	modelFile.offset += indexDataSize;
}

VKGeometry::StaticMesh load_dmf_static_mesh(StrA modelFileName) {
	MemoryArena& stackArena = get_scratch_arena();
	U64 stackArenaFrame0 = stackArena.stackPtr;
	U32 modelFileSize;
	Byte* modelData = read_full_file_to_arena<Byte>(&modelFileSize, stackArena, modelFileName);
	if (modelData == nullptr) {
		print("Failed to read model file: ");
		println(modelFileName);
		abort("Failed to read model file");
	}
	ByteBuf modelFile{};
	modelFile.wrap(modelData, modelFileSize);
	if (modelFile.read_u32() != bswap32('DUCK')) {
		abort("Model file header did not match DUCK");
	}
	if (modelFile.read_u32() < LAST_KNOWN_DMF_VERSION) {
		abort("Model file out of date");
	}
	U32 numObjects = modelFile.read_u32();
	if (numObjects == 0) {
		abort("No objects in file");
	}

	VKGeometry::StaticMesh mesh{};
	DMFObjectID objectType = static_cast<DMFObjectID>(modelFile.read_u32());
	if (objectType != DMF_OBJECT_ID_MESH) {
		abort("Tried to load non static mesh from file to static mesh");
	}
	StrA meshName = modelFile.read_stra();
	read_and_upload_dmf_mesh(&mesh, modelFile, nullptr);

	if (modelFile.failed) {
		abort("Reading model file failed");
	}
	stackArena.stackPtr = stackArenaFrame0;
	return mesh;
}

VKGeometry::SkeletalMesh load_dmf_skeletal_mesh(StrA modelFileName) {
	MemoryArena& stackArena = get_scratch_arena();
	U64 stackArenaFrame0 = stackArena.stackPtr;
	U32 modelFileSize;
	Byte* modelData = read_full_file_to_arena<Byte>(&modelFileSize, stackArena, modelFileName);
	if (modelData == nullptr) {
		print("Failed to read model file: ");
		println(modelFileName);
		abort("Failed to read model file");
	}
	ByteBuf modelFile{};
	modelFile.wrap(modelData, modelFileSize);
	if (modelFile.read_u32() != bswap32('DUCK')) {
		abort("Model file header did not match DUCK");
	}
	if (modelFile.read_u32() < LAST_KNOWN_DMF_VERSION) {
		abort("Model file out of date");
	}
	U32 numObjects = modelFile.read_u32();
	if (numObjects == 0) {
		abort("No objects in file");
	}

	VKGeometry::SkeletalMesh mesh{};
	DMFObjectID objectType = static_cast<DMFObjectID>(modelFile.read_u32());
	if (objectType != DMF_OBJECT_ID_ANIMATED_MESH) {
		abort("Tried to load non animated mesh from file to skeletal mesh");
	}
	StrA name = modelFile.read_stra();
	U32 numBones = modelFile.read_u32();
	U32 skeletonSize = OFFSET_OF(VKGeometry::Skeleton, bones[numBones]);
	VKGeometry::Skeleton* skeleton = globalArena.alloc_bytes<VKGeometry::Skeleton>(skeletonSize);
	skeleton->boneCount = numBones;

	for (U32 boneIdx = 0; boneIdx < numBones; boneIdx++) {
		StrA boneName = modelFile.read_stra();
		U32 parentIdx = modelFile.read_u32();
		if (parentIdx != VKGeometry::Bone::PARENT_INVALID_IDX && parentIdx >= boneIdx) {
			abort("Bone parent index out of range (parents must be written before children)");
		}
		VKGeometry::Bone& bone = skeleton->bones[boneIdx];
		bone.parentIdx = parentIdx;
		bone.bindTransform = modelFile.read_m4x3f32();
		if (parentIdx == VKGeometry::Bone::PARENT_INVALID_IDX) {
			bone.invBindTransform = bone.bindTransform;
		} else {
			M4x3F32& parentBindTransform = skeleton->bones[parentIdx].invBindTransform; // the bind transforms aren't yet inverted
			bone.invBindTransform = parentBindTransform * bone.bindTransform;
		}
	}
	for (U32 boneIdx = 0; boneIdx < numBones; boneIdx++) {
		skeleton->bones[boneIdx].invBindTransform.invert();
	}

	U32 numSubMeshes = modelFile.read_u32();
	if (numSubMeshes < 1) {
		abort("Animated skeleton had no meshes to animate");
	}
	read_and_upload_dmf_mesh(&mesh.geometry, modelFile, &mesh.skinningDataOffset);
	mesh.skeletonData = skeleton;

	if (modelFile.failed) {
		abort("Reading model file failed");
	}
	stackArena.stackPtr = stackArenaFrame0;
	return mesh;
}

VKGeometry::SkeletalAnimation load_daf(StrA animationFileName) {
	MemoryArena& stackArena = get_scratch_arena();
	U64 stackArenaFrame0 = stackArena.stackPtr;
	U32 modelFileSize;
	Byte* modelData = read_full_file_to_arena<Byte>(&modelFileSize, stackArena, animationFileName);
	if (modelData == nullptr) {
		print("Failed to read animation file: ");
		println(animationFileName);
		abort("Failed to read animation file");
	}
	ByteBuf modelFile{};
	modelFile.wrap(modelData, modelFileSize);
	if (modelFile.read_u32() != bswap32('DUCK')) {
		abort("Animation file header did not match DUCK");
	}
	if (modelFile.read_u32() < LAST_KNOWN_DAF_VERSION) {
		abort("Animation file out of date");
	}
	VKGeometry::SkeletalAnimation anim;
	anim.keyframeCount = modelFile.read_u32();
	anim.boneCount = modelFile.read_u32();
	anim.framerate = modelFile.read_f32();
	anim.lengthMilliseconds = modelFile.read_u32();
	anim.matrices = globalArena.alloc<M4x3F32>(anim.boneCount * anim.keyframeCount);
	for (U32 frame = 0; frame < anim.keyframeCount; frame++) {
		for (U32 bone = 0; bone < anim.boneCount; bone++) {
			anim.matrices[frame * anim.boneCount + bone] = modelFile.read_m4x3f32();
		}
	}

	if (modelFile.failed) {
		abort("Reading model file failed");
	}
	stackArena.stackPtr = stackArenaFrame0;
	return anim;
}


const U32 MAX_TEXTURE_COUNT = 128 * 1024;
U32 currentTextureMaxCount = 128;
U32 currentTextureCount = 0;

enum TextureFormat : U32 {
	TEXTURE_FORMAT_NULL,
	TEXTURE_FORMAT_RGBA_U8,
	TEXTURE_FORMAT_RGBA_U16,
	TEXTURE_FORMAT_RGBA_BC7,
	TEXTURE_FORMAT_COUNT
};
// I forgot what this enum was for. Perhaps to control mipmap generation? SRGB vs linear? I'm sure I needed it for something
enum TextureType : U32 {
	TEXTURE_TYPE_COLOR,
	TEXTURE_TYPE_NORMAL_MAP,
	TEXTURE_TYPE_MSDF
};
struct Texture {
	VkImage image;
	VkImageView imageView;
	U32 index;
	TextureType type;
};

Texture missing;
Texture simpleWhite;

ArenaArrayList<Texture*> allTextures;
ArenaArrayList<VkDeviceMemory> memoryUsed;
const U64 blockAllocationSize = 32 * MEGABYTE;
U64 currentMemoryBlockOffset = 0;

void alloc_texture_memory(VkDeviceMemory* memoryOut, VkDeviceSize* allocatedOffsetOut, VkMemoryRequirements memoryRequirements) {
	if (!(memoryRequirements.memoryTypeBits & (1 << VK::deviceMemoryTypeIndex))) {
		abort("Texture can't be in device memory? Shouldn't happen.");
	}
	if (memoryUsed.empty() || I64(blockAllocationSize - ALIGN_HIGH(currentMemoryBlockOffset, memoryRequirements.alignment)) < I64(memoryRequirements.size)) {
		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = max(blockAllocationSize, memoryRequirements.size);
		allocInfo.memoryTypeIndex = VK::deviceMemoryTypeIndex;
		VkDeviceMemory memory;
		CHK_VK(VK::vkAllocateMemory(VK::logicalDevice, &allocInfo, nullptr, &memory));
		memoryUsed.push_back(memory);
		currentMemoryBlockOffset = 0;
	}
	U64 allocatedOffset = ALIGN_HIGH(currentMemoryBlockOffset, memoryRequirements.alignment);
	currentMemoryBlockOffset = allocatedOffset + memoryRequirements.size;

	*memoryOut = memoryUsed.back();
	*allocatedOffsetOut = allocatedOffset;
}

void create_texture(Texture* result, void* data, U32 width, U32 height, TextureFormat format, TextureType type) {
	Texture& tex = *result;
	tex.type = type;
	VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = type == TEXTURE_TYPE_COLOR ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
	imageCreateInfo.extent = VkExtent3D{ width, height, 1 };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	CHK_VK(VK::vkCreateImage(VK::logicalDevice, &imageCreateInfo, nullptr, &tex.image));

	VkMemoryRequirements memoryRequirements;
	VK::vkGetImageMemoryRequirements(VK::logicalDevice, tex.image, &memoryRequirements);

	VkDeviceMemory linearBlock;
	VkDeviceSize memoryOffset;
	alloc_texture_memory(&linearBlock, &memoryOffset, memoryRequirements);
	CHK_VK(VK::vkBindImageMemory(VK::logicalDevice, tex.image, linearBlock, memoryOffset));

	VkCommandBuffer stagingCmdBuf = VK::graphicsStager.prepare_for_image_upload(width, height, sizeof(RGBA8));

	VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK::graphicsFamily;
	barrier.dstQueueFamilyIndex = VK::graphicsFamily;
	barrier.image = tex.image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	VK::vkCmdPipelineBarrier(stagingCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	if (data) {
		VK::graphicsStager.upload_to_image(tex.image, data, width, height, sizeof(RGBA8), 0);

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_NONE_KHR;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		VK::vkCmdPipelineBarrier(stagingCmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	VkImageViewCreateInfo imageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	imageViewCreateInfo.image = tex.image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = imageCreateInfo.format;
	imageViewCreateInfo.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	imageViewCreateInfo.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imageViewCreateInfo, nullptr, &tex.imageView));

	allTextures.push_back(result);

	if (currentTextureCount == currentTextureMaxCount) {
		currentTextureMaxCount *= 2;
		VK::drawDataDescriptorSet.change_dynamic_array_length(currentTextureMaxCount);
	}

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstSet = VK::drawDataDescriptorSet.descriptorSet;
	write.dstBinding = VK::drawDataDescriptorSet.bindingCount - 1;
	write.dstArrayElement = currentTextureCount;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	VkDescriptorImageInfo imageInfo;
	imageInfo.sampler = VK_NULL_HANDLE;
	imageInfo.imageView = tex.imageView;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	write.pImageInfo = &imageInfo;
	VK::vkUpdateDescriptorSets(VK::logicalDevice, 1, &write, 0, nullptr);

	tex.index = currentTextureCount;
	currentTextureCount++;
}

void load_png(Texture* result, StrA path) {
	MemoryArena& stackArena = get_scratch_arena();
	MEMORY_ARENA_FRAME(stackArena) {
		RGBA8* image;
		U32 width, height;
		PNG::read_image(stackArena, &image, &width, &height, path);
		if (image) {
			create_texture(result, image, width, height, TEXTURE_FORMAT_RGBA_U8, TEXTURE_TYPE_COLOR);
		} else {
			*result = missing;
		}
	}
}

void load_msdf(Texture* result, StrA path) {
	MemoryArena& stackArena = get_scratch_arena();
	MEMORY_ARENA_FRAME(stackArena) {
		U32 fileSize = 0;
		Byte* file = read_full_file_to_arena<Byte>(&fileSize, stackArena, path);
		if (file) {
			U32 width = LOAD_LE32(file);
			U32 height = LOAD_LE32(file + sizeof(U32));
			create_texture(result, file + sizeof(U32) * 2, width, height, TEXTURE_FORMAT_RGBA_U8, TEXTURE_TYPE_MSDF);
		} else {
			*result = missing;
		}

	}
}

void init_textures() {
	allTextures.reserve(256);
	memoryUsed.reserve(256);

	RGBA8 hardcodedTextureData[16 * 16];
	// The classic purple and black checker pattern
	for (U32 y = 0; y < 16; y++) {
		for (U32 x = 0; x < 16; x++) {
			hardcodedTextureData[y * 16 + x] = x < 8 == y < 8 ? RGBA8{ 255, 0, 255, 255 } : RGBA8{ 0, 0, 0, 255 };
		}
	}
	create_texture(&missing, hardcodedTextureData, 16, 16, TEXTURE_FORMAT_RGBA_U8, TEXTURE_TYPE_COLOR);
	memset(hardcodedTextureData, 0xFF, sizeof(hardcodedTextureData));
	create_texture(&simpleWhite, hardcodedTextureData, 16, 16, TEXTURE_FORMAT_RGBA_U8, TEXTURE_TYPE_COLOR);
}

void cleanup_textures() {
	for (Texture* tex : allTextures) {
		VK::vkDestroyImageView(VK::logicalDevice, tex->imageView, nullptr);
		VK::vkDestroyImage(VK::logicalDevice, tex->image, nullptr);
	}
	allTextures.clear();
	for (VkDeviceMemory mem : memoryUsed) {
		VK::vkFreeMemory(VK::logicalDevice, mem, nullptr);
	}
	memoryUsed.clear();
}

}