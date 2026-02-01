#pragma once
#include "ResourceLoading.h"

namespace Resources {

using namespace ResourceLoading;

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
VKGeometry::StaticMesh cannonMesh;
VKGeometry::StaticMesh matMesh;

Texture trowbridgeReitzBRDFLut;
Texture specularCubemap;
Texture diffuseCubemap;

Texture fontAtlas;
Texture uiIncrementLeft;
Texture uiIncrementRight;

Material cannonMat;
Material matMat;

void load_resources() {
	testMesh = load_dmf_static_mesh("./resources/models/test_level.dmf"a);
	testAnimMesh = load_dmf_skeletal_mesh("./resources/models/test_anim.dmf"a);
	rightHandMesh = load_dmf_skeletal_mesh("./resources/models/right_hand.dmf"a);
	rightHandCloseAnim = load_daf("./resources/models/right_hand_close.daf"a);
	ASSERT(rightHandCloseAnim.boneCount == rightHandMesh.skeletonData->boneCount, "rightHandCloseAnim animation bone count did not match rightHandMesh bone count");
	leftHandMesh = load_dmf_skeletal_mesh("./resources/models/left_hand.dmf"a);
	cannonMesh = load_dmf_static_mesh("resources/models/cannon.dmf"a);
	matMesh = load_dmf_static_mesh("resources/models/mat.dmf"a);
	
	load_dtf(&trowbridgeReitzBRDFLut, "./resources/textures/trowbridge_reitz_lut.dtf"a);
	load_dtf(&specularCubemap, "./resources/textures/city_specular.dtf"a);
	load_dtf(&diffuseCubemap, "./resources/textures/city_diffuse.dtf"a);
	VK::set_pbr_params(specularCubemap.imageView, diffuseCubemap.imageView, trowbridgeReitzBRDFLut.imageView);

	load_png(&uiIncrementLeft, "resources/textures/ui_increment_left.png"a);
	load_png(&uiIncrementRight, "resources/textures/ui_increment_right.png"a);
	load_msdf(&fontAtlas, "resources/textures/font2.ddf"a);

	create_material_from_pngs(&cannonMat, "resources/textures/cannon"a);
	create_material_from_pngs(&matMat, "resources/textures/mat"a);
}


}