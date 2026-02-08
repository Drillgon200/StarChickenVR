#pragma once
#include "DrillLib.h"
#include "Win32.h"
#include "VK.h"
#include "XR.h"
#include "Resources.h"
#include "StarChicken_decl.h"

namespace Level {

M4x3F* testAnimPose;

const U32 INVALID_LEVEL_OBJECT_ID = 0;

enum LevelObjectType {
	LEVEL_OBJECT_EMPTY,
	LEVEL_OBJECT_STATIC_MODEL,
	LEVEL_OBJECT_SKELETAL_MODEL
};

struct LevelObject {
	LevelObjectType type;
	U32 id;
	enum {
		INVISIBLE = 1 << 0,
		SELECTED = 1 << 1
	};
	Flags32 flags;
	U32 typeGroupArrayIdx; // index into an array such as Level::staticModels. This is to make grouping by object type easier for compute passes
	M4x3F transform;
};

struct StaticModel {
	LevelObject obj;
	VKGeometry::StaticMesh* mesh;
	ResourceLoading::Material* material;
	U32 gpuMatrixIdx;
};

struct SkeletalModel {
	LevelObject obj;
	VKGeometry::SkeletalMesh* mesh;
	ResourceLoading::Material* material;
	M4x3F32* poseMatrices;
	U32 gpuMatrixIdx;
	U32 skinnedVerticesOffset;
	U32 skeletonMatrixOffset;
};

PoolAllocator<StaticModel> staticModelAllocator;
PoolAllocator<SkeletalModel> skeletalModelAllocator;

struct Level {
	U32 nextObjId;
	ArenaArrayList<U32> freeObjectIds;
	ArenaArrayList<StaticModel*> staticModels;
	ArenaArrayList<SkeletalModel*> skeletalModels;
	ArenaHashMap<U32, LevelObject*> idToLevelObject;
	ArenaArrayList<LevelObject*> selectedObjects;
	LevelObject* activeObject;

	void init() {
		nextObjId = 1;
	}

	U32 get_next_id() {
		return freeObjectIds.empty() ? nextObjId++ : freeObjectIds.pop_back();
	}

	void free_object(LevelObject* object) {
		idToLevelObject.remove(object->id);
		freeObjectIds.push_back(object->id);
		object->id = INVALID_LEVEL_OBJECT_ID;
		switch (object->type) {
		case LEVEL_OBJECT_EMPTY: break;
		case LEVEL_OBJECT_STATIC_MODEL: {
			U32 staticModelIdx = object->typeGroupArrayIdx;
			staticModels.data[staticModelIdx] = staticModels.pop_back();
			staticModels.data[staticModelIdx]->obj.typeGroupArrayIdx = staticModelIdx;
			staticModelAllocator.free((StaticModel*)object);
		} break;
		case LEVEL_OBJECT_SKELETAL_MODEL: {
			U32 skeletalModelIdx = object->typeGroupArrayIdx;
			skeletalModels.data[skeletalModelIdx] = skeletalModels.pop_back();
			skeletalModels.data[skeletalModelIdx]->obj.typeGroupArrayIdx = skeletalModelIdx;
			skeletalModelAllocator.free((SkeletalModel*)object);
		} break;
		}
	}

	void deselect_all() {
		for (LevelObject* obj : selectedObjects) {
			obj->flags &= ~Flags32(LevelObject::SELECTED);
		}
		selectedObjects.clear();
		activeObject = nullptr;
	}
	void select_object(U32 id) {
		if (LevelObject* obj = idToLevelObject.find_or_default(id, nullptr)) {
			activeObject = obj;
			if (!(obj->flags & LevelObject::SELECTED)) {
				selectedObjects.push_back(obj);
				obj->flags |= LevelObject::SELECTED;
			}
		}
	}
	void deselect_object(U32 id) {
		if (LevelObject* obj = idToLevelObject.find_or_default(id, nullptr)) {
			if (obj->flags & LevelObject::SELECTED) {
				if (activeObject == obj) {
					activeObject = nullptr;
				}
				selectedObjects.remove_obj_unordered(obj);
				obj->flags ^= LevelObject::SELECTED;
			}
		}
	}
	void select_all(U32* ids, U32 count) {
		for (U32* id = ids; id != ids + count; id++) {
			if (LevelObject* obj = idToLevelObject.find_or_default(*id, nullptr)) {
				if (!(obj->flags & LevelObject::SELECTED)) {
					selectedObjects.push_back(obj);
					obj->flags |= LevelObject::SELECTED;
				}
			}
		}
	}

	void prepare_render_transforms() {
		for (StaticModel* model : staticModels) {
			if (model->obj.flags & LevelObject::INVISIBLE) {
				continue;
			}
			model->gpuMatrixIdx = VK::uniformMatricesHandler.alloc(1);
			if (model->gpuMatrixIdx != 0) {
				VK::uniformMatricesHandler.matrixMemoryMapping[model->gpuMatrixIdx] = model->obj.transform;
			}
		}
		for (SkeletalModel* model : skeletalModels) {
			if (model->obj.flags & LevelObject::INVISIBLE) {
				continue;
			}
			model->gpuMatrixIdx = VK::uniformMatricesHandler.alloc(1);
			model->skeletonMatrixOffset = VK::uniformMatricesHandler.alloc(model->mesh->skeletonData->boneCount);
			model->skinnedVerticesOffset = VK::geometryHandler.alloc_skinned_result(model->mesh->geometry.verticesCount);
			if (model->gpuMatrixIdx != 0) {
				VK::uniformMatricesHandler.matrixMemoryMapping[model->gpuMatrixIdx] = model->obj.transform;
			}
			if (model->skeletonMatrixOffset != 0) {
				U32 boneCount = model->mesh->skeletonData->boneCount;
				M4x3F* matrices = get_scratch_arena().alloc<M4x3F>(0);
				if (model->poseMatrices) {
					VKGeometry::Bone* bones = model->mesh->skeletonData->bones;
					for (U32 i = 0; i < boneCount; i++) {
						if (bones[i].parentIdx == VKGeometry::Bone::PARENT_INVALID_IDX) {
							matrices[i] = model->poseMatrices[i];
						} else {
							matrices[i] = matrices[bones[i].parentIdx] * model->poseMatrices[i];
						}
					}
					for (U32 i = 0; i < boneCount; i++) {
						matrices[i] = matrices[i] * model->mesh->skeletonData->bones[i].invBindTransform;
					}
				} else {
					for (U32 i = 0; i < boneCount; i++) {
						matrices[i].set_identity();
					}
				}
				for (U32 i = 0; i < boneCount; i++) {
					VK::uniformMatricesHandler.matrixMemoryMapping[model->skeletonMatrixOffset + i] = matrices[i];
				}
			}
		}
	}

	void draw_models(VkCommandBuffer cmdBuf, U32 camIdx) {
		VK::DrawPushData drawData{ .drawSet = VK::defaultDrawDescriptorSet };
		VK_PUSH_MEMBER(cmdBuf, drawData, drawSet);
		for (StaticModel* model : staticModels) {
			if (model->obj.flags & LevelObject::INVISIBLE) {
				continue;
			}
			U32 selectionObjId = model->obj.flags & LevelObject::SELECTED ? model->obj.id | 0x80000000u : model->obj.id; // high bit set in the id buffer indicates this object is selected (used for selection outline)
			drawData.drawConstants.transformIdx = model->gpuMatrixIdx;
			drawData.drawConstants.verticesOffset = I32(model->mesh->verticesOffset + 1);
			drawData.drawConstants.camIdx = camIdx;
			drawData.drawConstants.objId = selectionObjId;
			drawData.drawConstants.materialId = model->material->gpuIdx;
			VK_PUSH_MEMBER(cmdBuf, drawData, drawConstants);
			VK::vkCmdDrawIndexed(cmdBuf, model->mesh->indicesCount, 1, model->mesh->indicesOffset, 0, 0);
		}

		for (SkeletalModel* model : skeletalModels) {
			if (model->obj.flags & LevelObject::INVISIBLE) {
				continue;
			}
			U32 selectionObjId = model->obj.flags & LevelObject::SELECTED ? model->obj.id | 0x80000000u : model->obj.id; // high bit set in the id buffer indicates this object is selected (used for selection outline)
			// Negate vertex offset so the shader knows to pull from the skinned vertex arrays
			drawData.drawConstants.transformIdx = model->gpuMatrixIdx;
			drawData.drawConstants.verticesOffset =  -I32(model->skinnedVerticesOffset + 1);
			drawData.drawConstants.camIdx = camIdx;
			drawData.drawConstants.objId = selectionObjId;
			drawData.drawConstants.materialId = model->material->gpuIdx;
			VK_PUSH_MEMBER(cmdBuf, drawData, drawConstants);
			VK::vkCmdDrawIndexed(cmdBuf, model->mesh->geometry.indicesCount, 1, model->mesh->geometry.indicesOffset, 0, 0);
		}
	}

	void init_level_object(LevelObject& obj, LevelObjectType type, M4x3F transform, U32 typeArrayIdx) {
		obj.type = type;
		obj.transform = transform;
		obj.id = get_next_id();
		idToLevelObject.insert(obj.id, &obj);
		obj.typeGroupArrayIdx = typeArrayIdx;
	}

	void add_static_model(VKGeometry::StaticMesh& mesh, ResourceLoading::Material& material, M4x3F transform) {
		StaticModel* model = staticModelAllocator.alloc();
		init_level_object(model->obj, LEVEL_OBJECT_STATIC_MODEL, transform, staticModels.size);
		model->mesh = &mesh;
		model->material = &material;
		staticModels.push_back(model);
	}

	void add_skeletal_model(VKGeometry::SkeletalMesh& mesh, ResourceLoading::Material& material, M4x3F transform, M4x3F* poseMatrices) {
		SkeletalModel* model = skeletalModelAllocator.alloc();
		init_level_object(model->obj, LEVEL_OBJECT_SKELETAL_MODEL, transform, skeletalModels.size);
		model->mesh = &mesh;
		model->material = &material;
		model->poseMatrices = poseMatrices;
		skeletalModels.push_back(model);
	}

	void update(F32 dt) {
		// I'll need to think up a decent animation system at some point
		VKGeometry::set_skeletal_default_pose(testAnimPose, Resources::testAnimMesh);
		testAnimPose[1].rotate_quat(QF32{}.from_axis_angle(AxisAngleF32{ V3F32_EAST, (sinf32(F32(StarChicken::totalTime) * 0.75F) + 1.0F) * 0.125F }));
	}
};

Level level;

void build_test_level() {
	level.init();
	level.add_static_model(Resources::testMesh, Resources::basicWhiteMaterial, M4x3F{}.set_identity());
	level.add_skeletal_model(Resources::testAnimMesh, Resources::basicWhiteMaterial, M4x3F{}.set_identity().translate(V3F{ 3.0F, 3.0F, 0.0F }), testAnimPose = VKGeometry::alloc_skeletal_default_pose(globalArena, Resources::testAnimMesh));
	level.add_static_model(Resources::cannonMesh, Resources::cannonMat, M4x3F{}.set_identity().translate(V3F{ 10.0F, 3.0F, 0.0F }));
	level.add_static_model(Resources::matMesh, Resources::matMat, M4x3F{}.set_identity().translate(V3F{ -20.0F, 3.0F, 0.0F }));

}

}