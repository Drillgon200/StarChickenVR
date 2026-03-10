#pragma once
#include "VKGeometry.h"
#include "VK_decl.h"
#include "PNG.h"

namespace ResourceLoading {

enum DMFObjectID : U32 {
	DMF_OBJECT_ID_NONE = 0,
	DMF_OBJECT_ID_MESH = 1,
	DMF_OBJECT_ID_ANIMATED_MESH = 2,
	DMF_OBJECT_ID_Count = 3
};

struct Entity {
	V3F pos;
};

static constexpr U32 LAST_KNOWN_DMF_VERSION = DRILL_LIB_MAKE_VERSION(3, 0, 0);
static constexpr U32 LAST_KNOWN_DAF_VERSION = DRILL_LIB_MAKE_VERSION(2, 0, 0);
static constexpr U32 LAST_KNOWN_DTF_VERSION = DRILL_LIB_MAKE_VERSION(2, 0, 0);

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

enum TextureFormat : U8 {
	TEXTURE_FORMAT_NULL,
	TEXTURE_FORMAT_RGBA_U8,
	TEXTURE_FORMAT_RG_U8,
	TEXTURE_FORMAT_RGBA_BC7,
	TEXTURE_FORMAT_R9G9B9E5,
	TEXTURE_FORMAT_COUNT
};
const U32 TEXTURE_FORMAT_TEXEL_SIZE[TEXTURE_FORMAT_COUNT]{ 0, 4, 2, 16, 4 };
enum TextureFlags : Flags16 {
	TEXTURE_FLAG_CUBE = 0b001,
	TEXTURE_FLAG_SRGB = 0b010,
	TEXTURE_FLAG_MSDF = 0b100
};
struct Texture {
	VkImage image;
	VkImageView imageView;
	Flags16 flags;
	U32 index;
};
#pragma pack(push, 1)
struct alignas(16) DTFHeader {
	char magic[4]; // DUCK
	U32 version;
	Flags16 flags;
	TextureFormat format;
	U8 mipCount;
	U16 width;
	U16 height;
};
#pragma pack(pop)

Texture missing;
Texture simpleWhite;
Texture simpleBlack;
Texture simpleNormal;
Texture simpleARM;

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

void create_texture(Texture* result, void* data, U32 width, U32 height, U32 mipLevels, TextureFormat format, bool isSRGB, bool isCube) {
	Texture& tex = *result;
	tex = {};
	VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.flags = isCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	VkFormat createFormat = VK_FORMAT_R8G8B8A8_UNORM;
	switch (format) {
	case TEXTURE_FORMAT_NULL: abort("Texture format cannot be null"); break;
	case TEXTURE_FORMAT_RGBA_U8: createFormat = isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM; break;
	case TEXTURE_FORMAT_RG_U8: createFormat = isSRGB ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM; break;
	case TEXTURE_FORMAT_RGBA_BC7: abort("BC7 not yet supported"); break;
	case TEXTURE_FORMAT_R9G9B9E5: createFormat = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32; break;
	default: abort("Unknown texture format"); break;
	}
	imageCreateInfo.format = createFormat;
	imageCreateInfo.extent = VkExtent3D{ width, height, 1 };
	imageCreateInfo.mipLevels = mipLevels;
	imageCreateInfo.arrayLayers = isCube ? 6 : 1;
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

	U32 imgDataSize = width * height * TEXTURE_FORMAT_TEXEL_SIZE[format];
	if (isCube) {
		imgDataSize *= 6;
	}
	VkCommandBuffer stagingCmdBuf = VK::graphicsStager.prepare_for_image_upload(imgDataSize);

	VK::img_barrier(stagingCmdBuf, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	if (data) {
		U32 mipWidth = width;
		U32 mipHeight = width;
		for (U32 mipLevel = 0; mipLevel < mipLevels; mipLevel++) {
			stagingCmdBuf = VK::graphicsStager.upload_to_image(tex.image, data, width, height, isCube ? 6 : 1, TEXTURE_FORMAT_TEXEL_SIZE[format], mipLevel);
			data = (Byte*)data + width * height * TEXTURE_FORMAT_TEXEL_SIZE[format] * (isCube ? 6 : 1);
			width = max(width / 2u, 1u);
			height = max(height / 2u, 1u);
		}
		VK::img_barrier(stagingCmdBuf, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_NONE_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	tex.imageView = VK::create_img_view(tex.image, isCube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D, createFormat, 0);

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

void load_png(Texture* result, StrA path, bool isSRGB = true) {
	MemoryArena& stackArena = get_scratch_arena();
	MEMORY_ARENA_FRAME(stackArena) {
		RGBA8* image;
		U32 width, height;
		PNG::read_image(stackArena, &image, &width, &height, path);
		if (image) {
			create_texture(result, image, width, height, 1, TEXTURE_FORMAT_RGBA_U8, isSRGB, false);
		} else {
			*result = missing;
		}
	}
}

void load_dtf(Texture* result, StrA path) {
	MemoryArena& stackArena = get_scratch_arena();
	MEMORY_ARENA_FRAME(stackArena) {
		stackArena.stackPtr = ALIGN_HIGH(stackArena.stackPtr, 16);
		U32 textureFileSize;
		Byte* textureData = read_full_file_to_arena<Byte>(&textureFileSize, stackArena, path);
		if (textureData == nullptr) {
			printf("Failed to read texture file: %\n"a, path);
			abort("Failed to read texture file");
		}
		ByteBuf textureFile{};
		textureFile.wrap(textureData, textureFileSize);
		if (textureFile.read_u32() != bswap32('DUCK')) {
			abort("Texture file header did not match DUCK");
		}
		if (textureFile.read_u32() < LAST_KNOWN_DAF_VERSION) {
			abort("Texture file out of date");
		}
		Flags16 flags = textureFile.read_u16();
		TextureFormat textureFormat = TextureFormat(textureFile.read_u8());
		U32 mipCount = textureFile.read_u8();
		U16 width = textureFile.read_u16();
		U16 height = textureFile.read_u16();
		create_texture(result, textureFile.bytes + textureFile.offset, width, height, mipCount, textureFormat, flags & TEXTURE_FLAG_SRGB, flags & TEXTURE_FLAG_CUBE);
		result->flags = flags;
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
			create_texture(result, file + sizeof(U32) * 2, width, height, 1, TEXTURE_FORMAT_RGBA_U8, false, false);
			result->flags = TEXTURE_FLAG_MSDF;
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
	create_texture(&missing, hardcodedTextureData, 16, 16, 1, TEXTURE_FORMAT_RGBA_U8, false, false);
	memset(hardcodedTextureData, 0xFF, sizeof(hardcodedTextureData));
	create_texture(&simpleWhite, hardcodedTextureData, 16, 16, 1, TEXTURE_FORMAT_RGBA_U8, false, false);
	memset(hardcodedTextureData, 0x00, sizeof(hardcodedTextureData));
	create_texture(&simpleBlack, hardcodedTextureData, 16, 16, 1, TEXTURE_FORMAT_RGBA_U8, false, false);
	//TODO good texture packing
	for (U32 i = 0; i < ARRAY_COUNT(hardcodedTextureData); i++) { hardcodedTextureData[i] = RGBA8{ 127, 127, 255, 255 }; };
	create_texture(&simpleNormal, hardcodedTextureData, 16, 16, 1, TEXTURE_FORMAT_RGBA_U8, false, false);
	for (U32 i = 0; i < ARRAY_COUNT(hardcodedTextureData); i++) { hardcodedTextureData[i] = RGBA8{ 255, 255, 0, 255 }; };
	create_texture(&simpleARM, hardcodedTextureData, 16, 16, 1, TEXTURE_FORMAT_RGBA_U8, false, false);
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

struct Material {
	Texture* baseColor;
	Texture* normalMap;
	Texture* armMap;
	V4F color;
	F32 ambientOcclusion;
	F32 roughness;
	F32 metallic;
	F32 ior;
	U32 gpuIdx;
};

#pragma pack(push, 1)
struct GPUMaterial {
	I32 baseColorIdx;
	I32 normalMapIdx;
	I32 armMapIdx;
	U32 packedBaseColor;
	U32 packedARMI; // ambient occlusion, roughness, metallic, IOR (1.0-5.0)
};
#pragma pack(pop)

VK::DedicatedBuffer materialsBuffer;
const U32 maxMaterialCount = 1024;
U32 materialCount;

Material missingMaterial;
Material basicWhiteMaterial;

void material_updated(const Material& mat) {
	GPUMaterial gpuMat{};
	gpuMat.baseColorIdx = mat.baseColor ? mat.baseColor->index : -1;
	gpuMat.normalMapIdx = mat.normalMap ? mat.normalMap->index : -1;
	gpuMat.armMapIdx = mat.armMap ? mat.armMap->index : -1;
	gpuMat.packedBaseColor = pack_unorm4x8(mat.color);
	gpuMat.packedARMI = pack_unorm4x8(V4F{ mat.ambientOcclusion, mat.roughness, mat.metallic, clamp01((mat.ior - 1.0F) * 0.25F) });
	((GPUMaterial*)materialsBuffer.mapping)[mat.gpuIdx] = gpuMat;
}

void create_material(Material* mat) {
	if (materialCount == maxMaterialCount) {
		abort("Ran out of materials");
	}
	U32 materialIdx = materialCount++;
	mat->gpuIdx = materialIdx;
	material_updated(*mat);
}
void create_material(Material* mat, V4F color, F32 roughness, F32 metallic) {
	*mat = {};
	mat->color = color;
	mat->ambientOcclusion = 1.0F;
	mat->roughness = roughness;
	mat->metallic = metallic;
	mat->ior = 1.3F;
	return create_material(mat);
}
void create_material(Material* mat, Texture* baseColor, Texture* normalMap, Texture* armMap) {
	*mat = {};
	mat->baseColor = baseColor;
	mat->normalMap = normalMap;
	mat->armMap = armMap;
	mat->color = V4F{ 1.0F, 1.0F, 1.0F, 1.0F };
	mat->ambientOcclusion = 1.0F;
	mat->roughness = 1.0F;
	mat->metallic = 0.0F;
	mat->ior = 1.3F;
	return create_material(mat);
}
void create_material_from_pngs(Material* mat, StrA pathBase) {
	Texture* textures = globalArena.alloc<Texture>(3);
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		load_png(&textures[0], stracat(arena, pathBase, "_BaseColor.png"a), true);
		load_png(&textures[1], stracat(arena, pathBase, "_Normal.png"a), false);
		load_png(&textures[2], stracat(arena, pathBase, "_ARM.png"a), false);
	}
	create_material(mat, &textures[0], &textures[1], &textures[2]);
}

VkDeviceAddress get_materials_gpu_address() {
	return materialsBuffer.gpuAddress;
}

void flush_materials_memory() {
	if (!(VK::deviceHostMappedMemoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
		VkMappedMemoryRange memoryRange{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		memoryRange.memory = materialsBuffer.memory;
		memoryRange.offset = 0;
		memoryRange.size = VK_WHOLE_SIZE;
		CHK_VK(VK::vkFlushMappedMemoryRanges(VK::logicalDevice, 1, &memoryRange));
	}
}

void init_materials(){
	materialsBuffer.create(maxMaterialCount * sizeof(GPUMaterial), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK::deviceHostMappedMemoryTypeIndex);
	create_material(&missingMaterial, &missing, &simpleNormal, &simpleARM);
	create_material(&basicWhiteMaterial, V4F{ 1.0F, 1.0F, 1.0F, 1.0F }, 1.0F, 0.0F);
}

void destroy_materials() {
	materialsBuffer.destroy();
}

}