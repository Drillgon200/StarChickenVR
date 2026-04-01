#version 2
#shader vertex
#extension multiview

[input, builtin VertexIndex] &I32 inVertexIndex;
[input, builtin ViewIndex] &I32 inViewIndex;

[output, builtin Position] &V4F outPosition;
[output, location 0] &V2F passPos;
[output, location 1, flat] &I32 passViewIndex;

[entrypoint] @[][] vert_main{
	I32 vertIdx{ ^inVertexIndex };
	V4F ndcPosition{ F32((vertIdx >>> 1) * 4 - 1), F32((vertIdx & 1) * 4 - 1), 0.5, 1.0 };
	^passPos = ndcPosition.xy;
	^passViewIndex = ^inViewIndex;
	^outPosition = ndcPosition;
};

//---------------------------------------------------------//
#shader fragment

#include "lib.dsi"

[output, location 0] &V4F outFragColor;
[output, location 1] &U32 outObjId;
[input, location 0] &V2F passPos;
[input, location 1, flat] &I32 passViewIndex;

[uniform, set 0, binding 0] &Sampler bilinearSampler;
[set 0, binding 2, uniform_buffer, restrict, nonwritable, block] &DrawData drawData;
[uniform, set 0, binding 3] &ImageCubeSampled backgroundCube;
[uniform, set 0, binding 4] &ImageCubeSampled diffuseCube;

[push_constant, block] &WorldDrawPushConstants drawPushData;

[entrypoint] @[][] frag_main{
	I32 viewIdx{ ^passViewIndex };
	Camera cam{ drawData.cams[viewIdx + I32(drawPushData.camIdx)] };
	V2F pos{ ^passPos };
	V3F worldDirection{ -(pos.x - cam.projXZBias) / cam.projXScale, (pos.y - cam.projYZBias) / cam.projYScale, 1.0 };
	worldDirection = normalize(worldDirection);
	worldDirection = worldDirection.x * cam.worldToView.row0.xyz + worldDirection.y * cam.worldToView.row1.xyz + worldDirection.z * cam.worldToView.row2.xyz;
	worldDirection = V3F(worldDirection.x, -worldDirection.y, worldDirection.z);

	^outFragColor = V4F((^diffuseCube)[^bilinearSampler, worldDirection, 0.0].rgb, 1.0);
	^outObjId = 0u;
};