#pragma once
#include "DrillLib.h"
#include "VK_decl.h"

namespace VKGeometry {

struct Bone {
	static constexpr u32 PARENT_INVALID_IDX = U32_MAX;

	Matrix4x3f bindTransform;
	Matrix4x3f invBindTransform;
	u32 parentIdx;
};

struct Skeleton {
	// Skeletons have a linked list of freed transforms. If the list is empty, allocate a new one
	u32 nextFreeTransformPointer;
	u32 boneCount;
	// Technically only valid in C, not C++, but MSVC supports it, and other compilers have extensions for flexible array members if I ever decide to switch
	Bone bones[];
};

struct StaticMesh {
	u32 indicesOffset;
	u32 verticesOffset;
	u32 indicesCount;
	u32 verticesCount;
};

struct SkeletalMesh {
	StaticMesh geometry;
	// Several meshes may have the same skeleton structure (e.g. multiple humanoid models animated the same way)
	Skeleton* skeletonData;
	u32 skinningDataOffset;
};

struct SkeletalAnimation {
	u32 keyframeCount;
	u32 boneCount;
	f32 framerate;
	u32 lengthMilliseconds;
	// Array of pose arrays
	// Array contains one pose array for each keyframe, and each pose array contains one matrix for each bone
	Matrix4x3f* matrices;
};

struct StaticModel {
	StaticMesh* mesh;
	Matrix4x3f transform;
	u32 gpuMatrixIndex;
};

struct SkeletalModel {
	SkeletalMesh* mesh;
	Matrix4x3f* poseMatrices;
	Matrix4x3f transform;
	u32 gpuMatrixIndex;
	u32 skinnedVerticesOffset;
	u32 skeletonMatrixOffset;
};

#pragma pack(push, 1)
struct GPUSkinnedModel {
	u32 matricesOffset;
	u32 vertexOffset;
	u32 skinnedVerticesOffset;
	u32 skinningDataOffset;
	u32 vertexCount;
};
#pragma pack(pop)

struct GeometryHandler {
	VkDeviceMemory memory;
	VkDeviceSize memorySize;
	VkBuffer buffer;
	u64 indicesOffset;
	u64 positionsOffset;
	u64 texcoordsOffset;
	u64 normalsOffset;
	u64 tangentsOffset;
	u64 skinDataOffset;
	u64 skinnedPositionsOffset;
	u64 skinnedNormalsOffset;
	u64 skinnedTangentsOffset;
	u32 indicesAllocationCap;
	u32 geometryVertexAllocationCap;
	u32 skinningVertexAllocationCap;
	u32 skinnedVertexAllocationCap;
	u32 indicesAllocationOffset;
	u32 geometryVertexAllocationOffset;
	u32 skinningVertexAllocationOffset;
	u32 skinnedVertexAllocationOffset;

	void init(VkDeviceSize size) {
		if (size < 4096) {
			abort("Geometry needs 4k bytes");
		}
		const u64 alignment = VK::physicalDeviceProperties.limits.minStorageBufferOffsetAlignment;
		const u64 indexSize = sizeof(u16);
		f64 vertexDataSizesTotal = f64(VK::VERTEX_FORMAT_POS3F_TEX2F_NORM3F_TAN3F_SIZE) + f64(VK::VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE) + f64(VK::VERTEX_FORMAT_POS3F_NORM3F_TAN3F_SIZE) + f64(indexSize);
		// Approximate weights for how relatively many vertices we need space for in each section.
		// I'll come back to this later when I have actual data and set them accordingly
		// I'm not actually sure if I want to keep with static allocation like this.
		// Dynamic allocation could work better, since that could conform memory allocation to gameplay needs.
		// Anyway, I'm not going to think too hard about it now
		f64 indicesDataScaledSize = 0.15 * (f64(indexSize) / vertexDataSizesTotal);
		f64 geometryDataScaledSize = 0.35 * (f64(VK::VERTEX_FORMAT_POS3F_TEX2F_NORM3F_TAN3F_SIZE) / vertexDataSizesTotal);
		f64 skinningDataScaledSize = 0.2 * (f64(VK::VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE) / vertexDataSizesTotal);
		f64 skinningGeometryScaledSize = 0.3 * (f64(VK::VERTEX_FORMAT_POS3F_NORM3F_TAN3F_SIZE) / vertexDataSizesTotal);

		u64 maxAllocatableIndices = min<u64>(U32_MAX, (u64(indicesDataScaledSize * f64(size)) - alignment) / indexSize);
		indicesAllocationCap = u32(maxAllocatableIndices);
		indicesOffset = 0;
		// Vertices must be at most I32_MAX, since 
		u64 maxAllocatableVertices = min<u64>(I32_MAX, (u64(geometryDataScaledSize * f64(size)) - alignment * 4) / VK::VERTEX_FORMAT_POS3F_TEX2F_NORM3F_TAN3F_SIZE);
		geometryVertexAllocationCap = u32(maxAllocatableVertices);
		positionsOffset = ALIGN_HIGH(indicesOffset + indicesAllocationCap * indexSize, alignment);
		texcoordsOffset = ALIGN_HIGH(positionsOffset + geometryVertexAllocationCap * sizeof(Vector3f), alignment);
		normalsOffset = ALIGN_HIGH(texcoordsOffset + geometryVertexAllocationCap * sizeof(Vector2f), alignment);
		tangentsOffset = ALIGN_HIGH(normalsOffset + geometryVertexAllocationCap * sizeof(Vector3f), alignment);
		u64 maxAllocatableSkinningIndicesAndWeights = min<u64>(I32_MAX, (u64(skinningDataScaledSize * f64(size)) - alignment) / VK::VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE);
		skinningVertexAllocationCap = u32(maxAllocatableSkinningIndicesAndWeights);
		skinDataOffset = ALIGN_HIGH(tangentsOffset + geometryVertexAllocationCap * sizeof(Vector3f), alignment);
		u64 maxAllocatableSkinnedVertices = min<u64>(I32_MAX, (u64(skinningGeometryScaledSize * f64(size)) - alignment * 3) / VK::VERTEX_FORMAT_POS3F_NORM3F_TAN3F_SIZE);
		skinnedVertexAllocationCap = u32(maxAllocatableSkinnedVertices);
		skinnedPositionsOffset = ALIGN_HIGH(skinDataOffset + skinningVertexAllocationCap * u64(VK::VERTEX_FORMAT_INDEX4u8_WEIGHT4unorm8_SIZE), alignment);
		skinnedNormalsOffset = ALIGN_HIGH(skinnedPositionsOffset + skinnedVertexAllocationCap * sizeof(Vector3f), alignment);
		skinnedTangentsOffset = ALIGN_HIGH(skinnedNormalsOffset + skinnedVertexAllocationCap * sizeof(Vector3f), alignment);
		u64 finalSizeRequired = skinnedTangentsOffset + skinnedVertexAllocationCap * sizeof(Vector3f);
		memorySize = finalSizeRequired;

		indicesAllocationOffset = 0;
		geometryVertexAllocationOffset = 0;
		skinningVertexAllocationOffset = 0;
		skinnedVertexAllocationOffset = 0;

		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.flags = 0;
		bufferInfo.size = finalSizeRequired;
		bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
		CHK_VK(VK::vkCreateBuffer(VK::logicalDevice, &bufferInfo, VK_NULL_HANDLE, &buffer));
		VkMemoryRequirements memoryRequirements;
		VK::vkGetBufferMemoryRequirements(VK::logicalDevice, buffer, &memoryRequirements);
		if (!(memoryRequirements.memoryTypeBits & (1 << VK::deviceMemoryTypeIndex))) {
			abort("Could not create geometry buffer in device local memory");
		}
		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = VK::deviceMemoryTypeIndex;
		CHK_VK(VK::vkAllocateMemory(VK::logicalDevice, &allocInfo, nullptr, &memory));
		CHK_VK(VK::vkBindBufferMemory(VK::logicalDevice, buffer, memory, 0));
	}

	void alloc_static(u32* indicesOffsetOut, u32* verticesOffsetOut, u32 numIndices, u32 numVertices) {
		if (numVertices > U16_MAX) {
			abort("More vertices than maximum index, split the model or implement 32 bit indices");
		}
		u32 indicesStart = indicesAllocationOffset;
		indicesAllocationOffset = indicesStart + numIndices;
		if (indicesAllocationOffset > indicesAllocationCap) {
			abort("Ran out of memory for indices buffer. Perhaps time to implement streaming?");
		}
		u32 verticesStart = geometryVertexAllocationOffset;
		geometryVertexAllocationOffset = verticesStart + numVertices;
		if (geometryVertexAllocationOffset > geometryVertexAllocationCap) {
			abort("Ran out of memory for geometry vertex buffer. Perhaps time to implement streaming?");
		}
		*indicesOffsetOut = indicesStart;
		*verticesOffsetOut = verticesStart;
	}

	void alloc_skeletal(u32* indicesOffsetOut, u32* verticesOffsetOut, u32* skinningDataOffset, u32 numIndices, u32 numVertices) {
		if (numVertices > U16_MAX) {
			abort("More vertices than maximum index, split the model or implement 32 bit indices");
		}
		u32 indicesStart = indicesAllocationOffset;
		indicesAllocationOffset = indicesStart + numIndices;
		if (indicesAllocationOffset > indicesAllocationCap) {
			abort("Ran out of memory for indices buffer. Perhaps time to implement streaming?");
		}
		u32 verticesStart = geometryVertexAllocationOffset;
		geometryVertexAllocationOffset = verticesStart + numVertices;
		if (geometryVertexAllocationOffset > geometryVertexAllocationCap) {
			abort("Ran out of memory for geometry vertex buffer. Perhaps time to implement streaming?");
		}
		u32 skinningStart = skinningVertexAllocationOffset;
		skinningVertexAllocationOffset = skinningStart + numVertices;
		if (skinningVertexAllocationOffset > skinningVertexAllocationCap) {
			abort("Ran out of memory for skinning data buffer. Perhaps time to implement streaming?");
		}
		*indicesOffsetOut = indicesStart;
		*verticesOffsetOut = verticesStart;
		*skinningDataOffset = skinningStart;
	}

	// Skinned results should be allocated each frame.
	// This means CPU culled objects don't use skinned memory, and a cheap bump allocator can be used.
	u32 alloc_skinned_result(u32 numVertices) {
		u32 skinnedVerticesStart = skinnedVertexAllocationOffset;
		skinnedVertexAllocationOffset += numVertices;
		if (skinnedVertexAllocationOffset > skinnedVertexAllocationCap) {
			abort("Ran out of memory for skinned vertices result. Think of a way to handle this without crashing.");
		}
		return skinnedVerticesStart;
	}

	void reset_skinned_results() {
		skinnedVertexAllocationOffset = 0;
	}

	void destroy() {
		VK::vkDestroyBuffer(VK::logicalDevice, buffer, VK_NULL_HANDLE);
		VK::vkFreeMemory(VK::logicalDevice, memory, nullptr);
	}
};

struct UniformMatricesHandler {
	VkDeviceMemory memory;
	VkBuffer buffer;
	Matrix4x3f* memoryMapping;
	u32 maxMatrices;
	// Matrix is always at least 3. The first matrix is the identity matrix and the next two are eye matrices
	u32 matrixOffset;

	void init(u32 size) {
		if (size < 4096) {
			abort("Needs 4k of matrix data");
		}
		maxMatrices = size / sizeof(Matrix4x3f);
		matrixOffset = 3;

		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.flags = 0;
		bufferInfo.size = size;
		bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
		CHK_VK(VK::vkCreateBuffer(VK::logicalDevice, &bufferInfo, nullptr, &buffer));
		VkMemoryRequirements memoryRequirements;
		VK::vkGetBufferMemoryRequirements(VK::logicalDevice, buffer, &memoryRequirements);
		if (!(memoryRequirements.memoryTypeBits & VK::hostMemoryTypeIndex)) {
			abort("Skinning matrix buffer did not support allocation in host memory");
		}
		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = VK::hostMemoryTypeIndex;
		CHK_VK(VK::vkAllocateMemory(VK::logicalDevice, &allocInfo, nullptr, &memory));
		CHK_VK(VK::vkBindBufferMemory(VK::logicalDevice, buffer, memory, 0));
		CHK_VK(VK::vkMapMemory(VK::logicalDevice, memory, 0, size, 0, reinterpret_cast<void**>(&memoryMapping)));

		memoryMapping[0] = Matrix4x3f{}.set_identity();
	}

	void destroy() {
		VK::vkDestroyBuffer(VK::logicalDevice, buffer, nullptr);
		VK::vkFreeMemory(VK::logicalDevice, memory, nullptr);
	}

	u32 alloc_and_set(u32 numMatrices, Matrix4x3f* matrices) {
		u32 offset = matrixOffset;
		matrixOffset += numMatrices;
		if (matrixOffset > maxMatrices) {
			print("Ran out of space for transform matrices. Figure out a way to deal with this.");
			offset = 0;
		} else {
			for (u32 i = 0; i < numMatrices; i++) {
				memoryMapping[offset + i] = matrices[i];
			}
		}
		return offset;
	}

	u32 alloc(u32 numMatrices) {
		u32 offset = matrixOffset;
		matrixOffset += numMatrices;
		if (matrixOffset > maxMatrices) {
			print("Ran out of space for transform matrices. Figure out a way to deal with this.");
			offset = 0;
		}
		return offset;
	}

	void set_eye_matrices(ProjectiveTransformMatrix left, ProjectiveTransformMatrix right) {
		// These two aren't really Matrix4x3fs, since the third row is omitted instead of the fourth, but they will fit in a Matrix4x3f
		memoryMapping[1] = Matrix4x3f{
			left.m00, left.m01, left.m02, left.m03,
			left.m10, left.m11, left.m12, left.m13,
			left.m30, left.m31, left.m32, left.m33
		};
		memoryMapping[2] = Matrix4x3f{
			right.m00, right.m01, right.m02, right.m03,
			right.m10, right.m11, right.m12, right.m13,
			right.m30, right.m31, right.m32, right.m33
		};
	}

	void flush_memory() {
		VkMappedMemoryRange memoryRange{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		memoryRange.memory = memory;
		memoryRange.offset = 0;
		memoryRange.size = ALIGN_HIGH(matrixOffset * sizeof(Matrix4x3f), VK::physicalDeviceProperties.limits.nonCoherentAtomSize);
		CHK_VK(VK::vkFlushMappedMemoryRanges(VK::logicalDevice, 1, &memoryRange));
	}

	void reset() {
		// First 3 are identity and two eye matrices
		matrixOffset = 3;
	}
};

void make_static_model(StaticModel* model, StaticMesh& mesh) {
	model->mesh = &mesh;
	model->transform.set_identity();
}

void make_skeletal_model(SkeletalModel* model, SkeletalMesh& mesh) {
	model->mesh = &mesh;
	model->transform.set_identity();
}

}