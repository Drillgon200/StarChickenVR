#version 2
#shader compute
#include "lib.dsi"

[set 0, binding 2, uniform_buffer, restrict, nonwritable, block] &DrawData drawData;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	U32 camIdx;
	U32 lightCount;
	F32 zNear;
	F32 zFar;
	F32 right;
	F32 left;
	F32 up;
	F32 down;
} pushConstants;

[input, builtin InvocationsPerSubgroup] &U32 invocationsPerSubgroup;
[input, builtin InvocationIdWithinSubgroup] &U32 invocationIdWithinSubgroup;
[input, builtin NumSubgroupsPerWorkgroup] &U32 numSubgroupsPerWorkgroup;
[input, builtin SubgroupIdWithinWorkgroup] &U32 subgroupIdWithinWorkgroup;
[input, builtin WorkgroupId] &V3U workgroupId;

[entrypoint, localsize 128 1 1] @[][] compute_main{
	// Idea from DOOM Eternal's presentation, one subgroup per tile, each subgroup writes lights linearly
	U32 lightCount{ pushConstants.lightCount };
	U32 subgroupSize{ ^invocationsPerSubgroup };
	U32 invocationId{ ^invocationIdWithinSubgroup };
	U32 subgroupCount{ ^numSubgroupsPerWorkgroup };
	U32 subgroupId{ ^subgroupIdWithinWorkgroup };

	U32 CLUSTER_RES_X{ 16u };
	U32 CLUSTER_RES_Y{ 16u };
	U32 CLUSTER_RES_Z{ 24u };
	U32 MAX_ITEMS_PER_CLUSTER{ 256u };
	U32 clusterIdx{ workgroupId.x * subgroupCount + subgroupId };
	if (clusterIdx >= CLUSTER_RES_X * CLUSTER_RES_Y * CLUSTER_RES_Z) {
		return;
	};
	U32 camIdx{ pushConstants.camIdx };
	U32 clusterGroupIdx{ CLUSTER_RES_X * CLUSTER_RES_Y * CLUSTER_RES_Z * camIdx };
	U32 clusterX{ clusterIdx % CLUSTER_RES_X };
	U32 clusterY{ clusterIdx / CLUSTER_RES_X % CLUSTER_RES_Y };
	U32 clusterZ{ clusterIdx / CLUSTER_RES_X / CLUSTER_RES_Y };
	
	F32 left{ pushConstants.left + (pushConstants.right - pushConstants.left) * (F32(clusterX) / F32(CLUSTER_RES_X)) };
	F32 right{ pushConstants.left + (pushConstants.right - pushConstants.left) * (F32(clusterX + 1u) / F32(CLUSTER_RES_X)) };
	F32 down{ pushConstants.down + (pushConstants.up - pushConstants.down) * (F32(clusterY) / F32(CLUSTER_RES_Y)) };
	F32 up{ pushConstants.down + (pushConstants.up - pushConstants.down) * (F32(clusterY + 1u) / F32(CLUSTER_RES_Y)) };
	// Same distribution as DOOM 2016. I'm a bit skeptical, since it seems to place way too many depth slices super close to the camera, but I'll experiment with it later
	F32 near{ pushConstants.zNear * pow(pushConstants.zFar / pushConstants.zNear, F32(clusterZ) / F32(CLUSTER_RES_Z)) };
	F32 far{ pushConstants.zNear * pow(pushConstants.zFar / pushConstants.zNear, F32(clusterZ + 1u) / F32(CLUSTER_RES_Z)) };
	if (clusterZ == CLUSTER_RES_Z - 1u) {
		far = 99999999.0;
	};

	V2F rightNormal{ normalize(V2F(right, 1.0)) };
	rightNormal = V2F(rightNormal.y, -rightNormal.x);
	V2F leftNormal{ normalize(V2F(left, 1.0F)) };
	leftNormal = V2F(-leftNormal.y, leftNormal.x);
	V2F upNormal{ normalize(V2F(up, 1.0F)) };
	upNormal = V2F(upNormal.y, -upNormal.x);
	V2F downNormal{ normalize(V2F(down, 1.0F)) };
	downNormal = V2F(-downNormal.y, downNormal.x);

	M4x3F worldToView{ drawData.cams[camIdx].worldToView };

	U32 unculledLightCount{ 0u };
	U32 iterations{ (lightCount + U32(^invocationsPerSubgroup) - 1u) / subgroupSize };

	for U32 i{ 0u }; i < iterations; i = i + 1u {
		U32 lightIdx{ i * subgroupSize + invocationId };
		Light light{ drawData.lights[lightIdx] };
		V3F lightPos{
			dot(V4F(light.pos, 1.0), worldToView.row0),
			dot(V4F(light.pos, 1.0), worldToView.row1),
			-dot(V4F(light.pos, 1.0), worldToView.row2)
		};
		F32 radius{ F32(light.packedCullRadiusAndType >> 2u) / 1024.0 };
		Bool intersects{
			lightPos.z + radius >= near &&
			lightPos.z - radius <= far &&
			dot(upNormal, V2F(lightPos.y, lightPos.z)) <= radius &&
			dot(downNormal, V2F(lightPos.y, lightPos.z)) <= radius &&
			dot(rightNormal, V2F(lightPos.x, lightPos.z)) <= radius &&
			dot(leftNormal, V2F(lightPos.x, lightPos.z)) <= radius
		};
		if intersects {
			V4U ballot{ subgroup_ballot(true) };
			U32 writeIdx{ unculledLightCount + subgroup_ballot_exclusive_bitcount(ballot) };
			drawData.clusterItems[(clusterGroupIdx + clusterIdx) * MAX_ITEMS_PER_CLUSTER + writeIdx].lightIdx = lightIdx;
			unculledLightCount = unculledLightCount + subgroup_ballot_bitcount(ballot);
		};
	};
	if subgroup_elect() {
		drawData.clusterBins[clusterGroupIdx + clusterIdx].lightCount = unculledLightCount;
	};
};