#pragma once
#include "DrillLib.h"
#include "Win32.h"
#include "VK.h"
#include "XR.h"
#include "Resources.h"
#include "StarChicken_decl.h"

namespace Level {

M4x3F* testAnimPose;

const U32 LAST_KNOWN_DLF_VERSION = DRILL_LIB_MAKE_VERSION(1, 0, 0);

const U32 INVALID_LEVEL_OBJECT_ID = 0;

enum LevelObjectType {
	LEVEL_OBJECT_EMPTY,
	LEVEL_OBJECT_STATIC_MODEL,
	LEVEL_OBJECT_SKELETAL_MODEL,
	LEVEL_OBJECT_LIGHT
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

enum LightType {
	LIGHT_TYPE_POINT,
	LIGHT_TYPE_SPOT,
	LIGHT_TYPE_DIRECTIONAL
};
struct Light {
	LevelObject obj;
	LightType type;
	V3F direction;
	F32 brightness;
	V3F color;
	U32 gpuMatrixIdx;
};

struct Prefab {
	LevelObject** objects;
	U32 objectCount;
	Rng3F32 boundingBox;
	ResourceLoading::Texture* icon;
};

ArenaArrayList<Prefab*> allPrefabs;

Rng3F32 expand_bounding_box_for_object(Rng3F32 boundingBox, LevelObject* obj) {
	switch (obj->type) {
	case LEVEL_OBJECT_EMPTY: break;
	case LEVEL_OBJECT_STATIC_MODEL:
	case LEVEL_OBJECT_SKELETAL_MODEL: {
		Rng3F32 objBox = obj->type == LEVEL_OBJECT_STATIC_MODEL ? ((StaticModel*)obj)->mesh->boundingBox : ((SkeletalModel*)obj)->mesh->geometry.boundingBox;
		boundingBox = boundingBox.unioned(objBox.transformed_bounds(obj->transform));
	} break;
	case LEVEL_OBJECT_LIGHT: {
		V3F pos = obj->transform.translation();
		boundingBox = boundingBox.unioned(Rng3F32{ pos.x, pos.y, pos.z, pos.x, pos.y, pos.z });
	} break;
	}
	return boundingBox;
}

void calc_prefab_bounding_box(Prefab* prefab) {
	Rng3F32 boundingBox{ F32_LARGE, F32_LARGE, F32_LARGE, -F32_LARGE, -F32_LARGE, -F32_LARGE };
	for (U32 i = 0; i < prefab->objectCount; i++) {
		LevelObject* obj = prefab->objects[i];
		boundingBox = expand_bounding_box_for_object(boundingBox, obj);
	}
	prefab->boundingBox = boundingBox;
}

U32 nextObjId = 1;
ArenaArrayList<U32> freeObjectIds;

U32 get_next_obj_id() {
	return freeObjectIds.empty() ? nextObjId++ : freeObjectIds.pop_back();
}

PoolAllocator<LevelObject> emptyAllocator;
PoolAllocator<StaticModel> staticModelAllocator;
PoolAllocator<SkeletalModel> skeletalModelAllocator;
PoolAllocator<Light> lightAllocator;

void init_level_object(LevelObject& obj, LevelObjectType type, M4x3F transform) {
	obj.type = type;
	obj.transform = transform;
	obj.id = get_next_obj_id();
}
void free_level_object(LevelObject* obj) {
	if (obj->id == INVALID_LEVEL_OBJECT_ID) {
		__debugbreak();
	}
	freeObjectIds.push_back(obj->id);
	obj->id = INVALID_LEVEL_OBJECT_ID;
	switch (obj->type) {
	case LEVEL_OBJECT_EMPTY: break;
	case LEVEL_OBJECT_STATIC_MODEL: staticModelAllocator.free((StaticModel*)obj); break;
	case LEVEL_OBJECT_SKELETAL_MODEL: skeletalModelAllocator.free((SkeletalModel*)obj); break;
	case LEVEL_OBJECT_LIGHT: lightAllocator.free((Light*)obj); break;
	}
}
StaticModel* get_static_model(VKGeometry::StaticMesh& mesh, ResourceLoading::Material& material, M4x3F transform) {
	StaticModel* model = staticModelAllocator.alloc();
	init_level_object(model->obj, LEVEL_OBJECT_STATIC_MODEL, transform);
	model->mesh = &mesh;
	model->material = &material;
	return model;
}
SkeletalModel* get_skeletal_model(VKGeometry::SkeletalMesh& mesh, ResourceLoading::Material& material, M4x3F transform, M4x3F* poseMatrices) {
	SkeletalModel* model = skeletalModelAllocator.alloc();
	init_level_object(model->obj, LEVEL_OBJECT_SKELETAL_MODEL, transform);
	model->mesh = &mesh;
	model->material = &material;
	model->poseMatrices = poseMatrices;
	return model;
}
Light* get_light(LightType lightType, V3F pos, V3F color, F32 brightness, V3F direction = V3F{}) {
	Light* light = lightAllocator.alloc();
	init_level_object(light->obj, LEVEL_OBJECT_LIGHT, M4x3F{}.set_identity().add_offset(pos));
	light->type = lightType;
	light->brightness = brightness;
	light->color = color;
	light->direction = direction;
	return light;
}
LevelObject* clone_object(LevelObject* obj) {
	LevelObject* result = nullptr;
	switch (obj->type) {
	case LEVEL_OBJECT_EMPTY: result = emptyAllocator.alloc(), *result = *obj; break;
	case LEVEL_OBJECT_STATIC_MODEL: result = &staticModelAllocator.alloc()->obj, *((StaticModel*)result) = *((StaticModel*)obj); break;
	case LEVEL_OBJECT_SKELETAL_MODEL: result = &skeletalModelAllocator.alloc()->obj, *((SkeletalModel*)result) = *((SkeletalModel*)obj); break;
	case LEVEL_OBJECT_LIGHT: result = &lightAllocator.alloc()->obj, *((Light*)result) = *((Light*)obj); break;
	}
	if (result) {
		result->id = get_next_obj_id();
		result->flags &= ~LevelObject::SELECTED;
		result->typeGroupArrayIdx = 0;
	} else {
		abort("Clone should be implemented"a);
	}
	return result;
}

Prefab* make_prefab(LevelObject** objects, U32 objectCount) {
	Prefab* prefab = globalArena.zalloc<Prefab>(1);
	prefab->objectCount = objectCount;
	prefab->objects = objects;
	calc_prefab_bounding_box(prefab);
	allPrefabs.push_back(prefab);
	return prefab;
}

Prefab* make_static_model_prefab(VKGeometry::StaticMesh& mesh, ResourceLoading::Material& mat) {
	LevelObject* meshObject = &get_static_model(mesh, mat, M4x3F{}.set_identity())->obj;
	Prefab* prefab = globalArena.zalloc<Prefab>(1);
	prefab->objectCount = 1;
	prefab->objects = globalArena.alloc<LevelObject*>(prefab->objectCount);
	prefab->objects[0] = meshObject;
	calc_prefab_bounding_box(prefab);
	allPrefabs.push_back(prefab);
	return prefab;
}

void make_default_prefabs() {
	make_static_model_prefab(Resources::testMesh, Resources::basicWhiteMaterial);
	make_static_model_prefab(Resources::cannonMesh, Resources::cannonMat);
	make_static_model_prefab(Resources::matMesh, Resources::matMat);
}

struct Level {
	ArenaArrayList<StaticModel*> staticModels;
	ArenaArrayList<SkeletalModel*> skeletalModels;
	ArenaArrayList<Light*> lights;
	ArenaHashMap<U32, LevelObject*> idToLevelObject;
	ArenaArrayList<LevelObject*> selectedObjects;
	LevelObject* activeObject;

	void reset() {
		for (StaticModel* obj : staticModels) { free_level_object(&obj->obj); }
		staticModels.clear();
		for (SkeletalModel* obj : skeletalModels) { free_level_object(&obj->obj); }
		skeletalModels.clear();
		for (Light* obj : lights) { free_level_object(&obj->obj); }
		lights.clear();
		idToLevelObject.clear();
		selectedObjects.clear();
		activeObject = nullptr;
	}

	V3F get_selection_midpoint() {
		V3F selectedMidpoint{};
		for (LevelObject* obj : selectedObjects) {
			selectedMidpoint += obj->transform.translation();
		}
		return selectedMidpoint / F32(selectedObjects.size);
	}
	Rng3F32 get_selection_bounding_box() {
		if (selectedObjects.empty()) {
			return Rng3F32{};
		}
		Rng3F32 boundingBox{ F32_LARGE, F32_LARGE, F32_LARGE, -F32_LARGE, -F32_LARGE, -F32_LARGE };
		for (LevelObject* obj : selectedObjects) {
			boundingBox = expand_bounding_box_for_object(boundingBox, obj);
		}
		return boundingBox;
	}
	void add_obj_to_selected(LevelObject* obj) {
		if (!(obj->flags & LevelObject::SELECTED)) {
			selectedObjects.push_back(obj);
			obj->flags |= LevelObject::SELECTED;
		}
	}
	void remove_obj_from_selected(LevelObject* obj) {
		if (obj->flags & LevelObject::SELECTED) {
			if (activeObject == obj) {
				activeObject = nullptr;
			}
			selectedObjects.remove_obj_unordered(obj);
			obj->flags &= ~Flags32(LevelObject::SELECTED);
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
			add_obj_to_selected(obj);
		}
	}
	void deselect_object(U32 id) {
		if (LevelObject* obj = idToLevelObject.find_or_default(id, nullptr)) {
			remove_obj_from_selected(obj);
		}
	}
	void select_objects(U32* ids, U32 count) {
		for (U32* id = ids; id != ids + count; id++) {
			if (LevelObject* obj = idToLevelObject.find_or_default(*id, nullptr)) {
				add_obj_to_selected(obj);
			}
		}
	}
	void select_all() {
		for (U32 i = 0; i < idToLevelObject.capacity; i++) {
			if (idToLevelObject.keys[i] != idToLevelObject.emptyKey) {
				add_obj_to_selected(idToLevelObject.values[i]);
			}
		}
	}

	void remove_object(LevelObject* obj) {
		remove_obj_from_selected(obj);
		idToLevelObject.remove(obj->id);
		switch (obj->type) {
		case LEVEL_OBJECT_STATIC_MODEL: {
			U32 staticModelIdx = obj->typeGroupArrayIdx;
			staticModels.data[staticModelIdx] = staticModels.pop_back();
			staticModels.data[staticModelIdx]->obj.typeGroupArrayIdx = staticModelIdx;
		} break;
		case LEVEL_OBJECT_SKELETAL_MODEL: {
			U32 skeletalModelIdx = obj->typeGroupArrayIdx;
			skeletalModels.data[skeletalModelIdx] = skeletalModels.pop_back();
			skeletalModels.data[skeletalModelIdx]->obj.typeGroupArrayIdx = skeletalModelIdx;
		} break;
		case LEVEL_OBJECT_LIGHT: {
			U32 lightIdx = obj->typeGroupArrayIdx;
			lights.data[lightIdx] = lights.pop_back();
			lights.data[lightIdx]->obj.typeGroupArrayIdx = lightIdx;
		} break;
		default: break;
		}
	}

	LevelObject* add_object(LevelObject* obj) {
		DEBUG_ASSERT(!idToLevelObject.contains(obj->id), "Object cannot be added twice"a);
		idToLevelObject.insert(obj->id, obj);
		obj->flags &= ~Flags32(LevelObject::SELECTED);
		switch (obj->type) {
		case LEVEL_OBJECT_STATIC_MODEL: {
			obj->typeGroupArrayIdx = staticModels.size;
			staticModels.push_back((StaticModel*)obj);
		} break;
		case LEVEL_OBJECT_SKELETAL_MODEL: {
			obj->typeGroupArrayIdx = skeletalModels.size;
			skeletalModels.push_back((StaticModel*)obj);
		} break;
		case LEVEL_OBJECT_LIGHT: {
			obj->typeGroupArrayIdx = lights.size;
			lights.push_back((StaticModel*)obj);
		} break;
		default: abort("Unknown object type"a); break;
		}
		return obj;
	}

	void free_object(LevelObject* obj) {
		remove_object(obj);
		free_level_object(obj);
	}

	void prepare_render() {
		for (StaticModel* model : staticModels) {
			if (model->obj.flags & LevelObject::INVISIBLE) {
				continue;
			}
			model->gpuMatrixIdx = VK::uniformDataHandler.alloc_matrices(1);
			if (model->gpuMatrixIdx != 0) {
				VK::uniformDataHandler.matrixMemoryMapping[model->gpuMatrixIdx] = model->obj.transform;
			}
		}
		for (SkeletalModel* model : skeletalModels) {
			if (model->obj.flags & LevelObject::INVISIBLE) {
				continue;
			}
			model->gpuMatrixIdx = VK::uniformDataHandler.alloc_matrices(1);
			model->skeletonMatrixOffset = VK::uniformDataHandler.alloc_matrices(model->mesh->skeletonData->boneCount);
			model->skinnedVerticesOffset = VK::geometryHandler.alloc_skinned_result(model->mesh->geometry.verticesCount);
			if (model->gpuMatrixIdx != 0) {
				VK::uniformDataHandler.matrixMemoryMapping[model->gpuMatrixIdx] = model->obj.transform;
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
					VK::uniformDataHandler.matrixMemoryMapping[model->skeletonMatrixOffset + i] = matrices[i];
				}
			}
		}
		for (Light* light : lights) {
			if (light->obj.flags & LevelObject::INVISIBLE) {
				continue;
			}
			VK::GPULight gpuLight{};
			gpuLight.pos = light->obj.transform.translation();
			gpuLight.direction = light->direction;
			gpuLight.color = light->color * light->brightness;
			F32 desiredZeroBrightness = 0.01F;
			F32 lightRadius = sqrtf32(max(max(light->color.x, light->color.y, light->color.z) * light->brightness / desiredZeroBrightness - 1.0F, 0.0F));
			gpuLight.packedCullRadiusAndType = U32(clamp(lightRadius * 1024.0F, 0.0F, F32((1 << 30) - 1))) << 2 | U32(light->type);
			VK::uniformDataHandler.alloc_light_and_set(gpuLight);
			light->gpuMatrixIdx = VK::uniformDataHandler.alloc_matrices(1);
			if (light->gpuMatrixIdx != 0) {
				VK::uniformDataHandler.matrixMemoryMapping[light->gpuMatrixIdx] = light->obj.transform;
			}
		}
	}

	void draw_models(VkCommandBuffer cmdBuf, U32 camIdx, M4x3F& viewMat, PerspectiveProjection& projection) {
		for (StaticModel* model : staticModels) {
			if (model->obj.flags & LevelObject::INVISIBLE) {
				continue;
			}
			V3F center = model->obj.transform * model->mesh->boundingBox.midpoint();
			if (projection.intersects_sphere(viewMat * center, model->mesh->boundingBox.diag_length() * 0.5F)) {
				U32 selectionObjId = model->obj.flags & LevelObject::SELECTED ? model->obj.id | 0x80000000u : model->obj.id; // high bit set in the id buffer indicates this object is selected (used for selection outline)
				VK::WorldDrawPushConstants modelInfo{ model->gpuMatrixIdx, I32(model->mesh->verticesOffset + 1), camIdx, selectionObjId, model->material->gpuIdx, VK::currentDebugDisplay };
				VK::vkCmdPushConstants(cmdBuf, VK::drawPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VK::WorldDrawPushConstants), &modelInfo);
				VK::vkCmdDrawIndexed(cmdBuf, model->mesh->indicesCount, 1, model->mesh->indicesOffset, 0, 0);
			}
		}

		for (SkeletalModel* model : skeletalModels) {
			if (model->obj.flags & LevelObject::INVISIBLE) {
				continue;
			}
			U32 selectionObjId = model->obj.flags & LevelObject::SELECTED ? model->obj.id | 0x80000000u : model->obj.id; // high bit set in the id buffer indicates this object is selected (used for selection outline)
			// Negate vertex offset so the shader knows to pull from the skinned vertex arrays
			VK::WorldDrawPushConstants modelInfo{ model->gpuMatrixIdx, -I32(model->skinnedVerticesOffset + 1), camIdx, selectionObjId, model->material->gpuIdx, VK::currentDebugDisplay };
			VK::vkCmdPushConstants(cmdBuf, VK::drawPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VK::WorldDrawPushConstants), &modelInfo);
			VK::vkCmdDrawIndexed(cmdBuf, model->mesh->geometry.indicesCount, 1, model->mesh->geometry.indicesOffset, 0, 0);
		}

		for (Light* light : lights) {
			U32 selectionObjId = light->obj.flags & LevelObject::SELECTED ? light->obj.id | 0x80000000u : light->obj.id; // high bit set in the id buffer indicates this object is selected (used for selection outline)
			VK::WorldDrawPushConstants modelInfo{ light->gpuMatrixIdx, I32(Resources::testSphere.verticesOffset + 1), camIdx, selectionObjId, Resources::simpleWhite.index, VK::currentDebugDisplay };
			VK::vkCmdPushConstants(cmdBuf, VK::drawPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VK::WorldDrawPushConstants), &modelInfo);
			VK::vkCmdDrawIndexed(cmdBuf, Resources::testSphere.indicesCount, 1, Resources::testSphere.indicesOffset, 0, 0);
		}
	}

	void update(F32 dt) {
		//TODO put a real animation system in
		VKGeometry::set_skeletal_default_pose(testAnimPose, Resources::testAnimMesh);
		testAnimPose[1].rotate_quat(QF32{}.from_axis_angle(AxisAngleF32{ V3F32_EAST, (sinf32(F32(StarChicken::totalTime) * 0.75F) + 1.0F) * 0.125F }));
	}
};

Level level;

void serialize_level_obj_base(ByteBuf& buf, LevelObject& obj) {
	buf.write_m4x3f32(obj.transform);
	buf.write_u32(obj.flags);
	buf.write_u32(U32(obj.type));
}
void deserialize_level_obj_base(LevelObject* obj, ByteBuf& buf) {
	obj->transform = buf.read_m4x3f32();
	obj->flags = buf.read_u32();
	obj->type = LevelObjectType(buf.read_u32());
}

void save_level(StrA outputPath, Level& lvl) {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		ArenaHashMap<StrA, U32> pathToPathId{ &arena };
		for (StaticModel* obj : lvl.staticModels) {
			pathToPathId.insert(obj->mesh->assetPath, 0);
			pathToPathId.insert(obj->material->assetPath, 0);
		}
		for (SkeletalModel* obj : lvl.skeletalModels) {
			pathToPathId.insert(obj->mesh->geometry.assetPath, 0);
			pathToPathId.insert(obj->material->assetPath, 0);
		}
		ByteBuf buf;
		buf.wrap(arena.stackBase + arena.stackPtr, GIGABYTE);
		buf.write_be32('DUCK');
		buf.write_u32(DRILL_LIB_MAKE_VERSION(1, 0, 0));

		buf.write_u32(pathToPathId.size);
		for (U32 i = 0, nextPathId = 0; i < pathToPathId.capacity; i++) {
			if (pathToPathId.keys[i] != pathToPathId.emptyKey) {
				pathToPathId.values[i] = nextPathId++;
				buf.write_stra(pathToPathId.keys[i]);
			}
		}

		buf.write_u32(lvl.staticModels.size);
		for (StaticModel* obj : lvl.staticModels) {
			serialize_level_obj_base(buf, obj->obj);
			buf.write_u32(*pathToPathId.find(obj->mesh->assetPath));
			buf.write_u32(*pathToPathId.find(obj->material->assetPath));
		}

		buf.write_u32(lvl.skeletalModels.size);
		for (SkeletalModel* obj : lvl.skeletalModels) {
			serialize_level_obj_base(buf, obj->obj);
			buf.write_u32(*pathToPathId.find(obj->mesh->geometry.assetPath));
			buf.write_u32(*pathToPathId.find(obj->material->assetPath));
			buf.write_u32(obj->mesh->skeletonData->boneCount);
			for (U32 i = 0; i < obj->mesh->skeletonData->boneCount; i++) {
				buf.write_m4x3f32(obj->poseMatrices[i]);
			}
		}

		buf.write_u32(lvl.lights.size);
		for (Light* obj : lvl.lights) {
			serialize_level_obj_base(buf, obj->obj);
			buf.write_u32(U32(obj->type));
			buf.write_v3f32(obj->direction);
			buf.write_f32(obj->brightness);
			buf.write_v3f32(obj->color);
		}

		arena.stackPtr += buf.offset;
		write_data_to_file(outputPath, buf.bytes, buf.offset);
	}
}
bool load_level(Level& lvl, StrA fileName) {
	bool success = false;
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		{
			U32 byteCount;
			Byte* data = read_full_file_to_arena<Byte>(&byteCount, arena, fileName);
			if (!data) {
				printf("Failed to load level: %"a, fileName);
				goto failed;
			}
			ByteBuf buf;
			buf.wrap(data, byteCount);
			if (buf.read_be32() != 'DUCK') {
				printf("Level magic did not match: %"a, fileName);
				goto failed;
			}
			if (buf.read_u32() != LAST_KNOWN_DLF_VERSION) {
				printf("Unsupported DLF version: %"a, fileName);
				goto failed;
			}

			lvl.reset();

			U32 assetPathCount = buf.read_u32();
			StrA* assetPaths = arena.alloc<StrA>(assetPathCount);
			for (U32 i = 0; i < assetPathCount; i++) {
				assetPaths[i] = buf.read_stra();
			}

			U32 staticModelCount = buf.read_u32();
			for (U32 i = 0; i < staticModelCount; i++) {
				StaticModel* obj = get_static_model(Resources::testMesh, Resources::missingMaterial, M4x3F{});
				deserialize_level_obj_base(&obj->obj, buf);
				obj->mesh = Resources::assetPathToStaticMesh.find_or_default(assetPaths[buf.read_u32()], obj->mesh);
				obj->material = Resources::assetPathToMaterial.find_or_default(assetPaths[buf.read_u32()], obj->material);
				lvl.add_object(&obj->obj);
			}

			U32 skeletalModelCount = buf.read_u32();
			for (U32 i = 0; i < skeletalModelCount; i++) {
				SkeletalModel* obj = get_skeletal_model(Resources::testAnimMesh, Resources::missingMaterial, M4x3F{}, nullptr);
				deserialize_level_obj_base(&obj->obj, buf);
				obj->mesh = Resources::assetPathToSkeletalMesh.find_or_default(assetPaths[buf.read_u32()], obj->mesh);
				obj->material = Resources::assetPathToMaterial.find_or_default(assetPaths[buf.read_u32()], obj->material);
				U32 boneCount = buf.read_u32();
				RUNTIME_ASSERT(boneCount == obj->mesh->skeletonData->boneCount, "Bone count did not match"a);
				M4x3F* poseMatrices = globalArena.alloc<M4x3F>(boneCount);
				for (U32 j = 0; j < boneCount; j++) {
					poseMatrices[j] = buf.read_m4x3f32();
				}
				obj->poseMatrices = poseMatrices;
				// I need to do a real animation system
				testAnimPose = poseMatrices;
				lvl.add_object(&obj->obj);
			}

			U32 lightCount = buf.read_u32();
			for (U32 i = 0; i < lightCount; i++) {
				Light* obj = get_light(LIGHT_TYPE_POINT, V3F{}, V3F{}, 0.0F, V3F{});
				deserialize_level_obj_base(&obj->obj, buf);
				obj->type = LightType(buf.read_u32());
				obj->direction = buf.read_v3f32();
				obj->brightness = buf.read_f32();
				obj->color = buf.read_v3f32();
				lvl.add_object(&obj->obj);
			}
			success = true;
		}
	failed:;
	}
	return success;
}

void build_test_level() {
	level.reset();
	level.add_object(&get_static_model(Resources::testMesh, Resources::basicWhiteMaterial, M4x3F{}.set_identity())->obj);
	level.add_object(&get_skeletal_model(Resources::testAnimMesh, Resources::basicWhiteMaterial, M4x3F{}.set_identity().translate(V3F{ 3.0F, 3.0F, 0.0F }), testAnimPose = VKGeometry::alloc_skeletal_default_pose(globalArena, Resources::testAnimMesh))->obj);
	level.add_object(&get_static_model(Resources::cannonMesh, Resources::cannonMat, M4x3F{}.set_identity().translate(V3F{ 10.0F, 3.0F, 0.0F }))->obj);
	level.add_object(&get_static_model(Resources::matMesh, Resources::matMat, M4x3F{}.set_identity().translate(V3F{ -20.0F, 3.0F, 0.0F }))->obj);
	level.add_object(&get_static_model(Resources::seag, Resources::seagMat, M4x3F{}.set_identity().translate(V3F{ -10.0F, 5.0F, 0.0F }))->obj);
	level.add_object(&get_light(LIGHT_TYPE_POINT, V3F{ 0.0F, 5.0F, 0.0F }, V3F{ 1.0F, 0.0F, 1.0F }, 10.0F)->obj);
}

}