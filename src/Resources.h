#pragma once
#include "VKGeometry_decl.h"
#include "VK_decl.h"

namespace Resources {

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

VKGeometry::StaticMesh testMesh;
VKGeometry::SkeletalMesh testAnimMesh;

#define BONE_INDEX_RIGHTHAND_WRIST 0
#define BONE_INDEX_RIGHTHAND_PALM 1
#define BONE_INDEX_RIGHTHAND_INDEXBASE 2
#define BONE_INDEX_RIGHTHAND_INDEXMIDDLE 3
#define BONE_INDEX_RIGHTHAND_INDEXTIP 4
#define BONE_INDEX_RIGHTHAND_MIDDLEBASE 5
#define BONE_INDEX_RIGHTHAND_MIDDLEMIDDLE 6
#define BONE_INDEX_RIGHTHAND_MIDDLETIP 7
#define BONE_INDEX_RIGHTHAND_RINGBASE 8
#define BONE_INDEX_RIGHTHAND_RINGMIDDLE 9
#define BONE_INDEX_RIGHTHAND_RINGTIP 10
#define BONE_INDEX_RIGHTHAND_PINKYBASE 11
#define BONE_INDEX_RIGHTHAND_PINKYMIDDLE 12
#define BONE_INDEX_RIGHTHAND_PINKYTIP 13
#define BONE_INDEX_RIGHTHAND_THUMBBASE 14
#define BONE_INDEX_RIGHTHAND_THUMBMIDDLE 15
#define BONE_INDEX_RIGHTHAND_THUMBTIP 16
VKGeometry::SkeletalMesh rightHandMesh;
VKGeometry::SkeletalAnimation rightHandCloseAnim;
#define BONE_INDEX_LEFTHAND_WRIST 0
#define BONE_INDEX_LEFTHAND_PALM 1
#define BONE_INDEX_LEFTHAND_INDEXBASE 2
#define BONE_INDEX_LEFTHAND_INDEXMIDDLE 3
#define BONE_INDEX_LEFTHAND_INDEXTIP 4
#define BONE_INDEX_LEFTHAND_MIDDLEBASE 5
#define BONE_INDEX_LEFTHAND_MIDDLEMIDDLE 6
#define BONE_INDEX_LEFTHAND_MIDDLETIP 7
#define BONE_INDEX_LEFTHAND_RINGBASE 8
#define BONE_INDEX_LEFTHAND_RINGMIDDLE 9
#define BONE_INDEX_LEFTHAND_RINGTIP 10
#define BONE_INDEX_LEFTHAND_PINKYBASE 11
#define BONE_INDEX_LEFTHAND_PINKYMIDDLE 12
#define BONE_INDEX_LEFTHAND_PINKYTIP 13
#define BONE_INDEX_LEFTHAND_THUMBBASE 14
#define BONE_INDEX_LEFTHAND_THUMBMIDDLE 15
#define BONE_INDEX_LEFTHAND_THUMBTIP 16
VKGeometry::SkeletalMesh leftHandMesh;



void load_resources() {
	testMesh = load_dmf_static_mesh("./resources/models/test_level.dmf"a);
	testAnimMesh = load_dmf_skeletal_mesh("./resources/models/test_anim.dmf"a);
	rightHandMesh = load_dmf_skeletal_mesh("./resources/models/right_hand.dmf"a);
	rightHandCloseAnim = load_daf("./resources/models/right_hand_close.daf"a);
	ASSERT(rightHandCloseAnim.boneCount == rightHandMesh.skeletonData->boneCount, "rightHandCloseAnim animation bone count did not match rightHandMesh bone count");
	leftHandMesh = load_dmf_skeletal_mesh("./resources/models/left_hand.dmf"a);
}


}