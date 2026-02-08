#version 2
#shader vertex
#extension multiview

#include "lib.dsi"

struct Vertex {
	V3F pos;
	U32 color;
};

[push_constant] &DrawPushData pushData;

[input, builtin VertexIndex] &I32 inVertexIndex;
[input, builtin ViewIndex] &I32 inViewIndex;
[output, builtin Position] &V4F outPosition;
[output, builtin PointSize] &F32 outPointSize;

[entrypoint] @[][] vert_main {
	&DrawData drawData{ UniformBuffer(DrawData)(pushData.drawSet.drawDataUniformBuffer) };
	&Vertex vertexBuffer{ (&Vertex)(v2u_u64_add(drawData.pUIVertices, U32(pushData.drawConstants.verticesOffset))) };
	Vertex vert{ vertexBuffer[^inVertexIndex] };

	I32 viewIdx{ ^inViewIndex };
	Camera cam{ drawData.cams[pushData.drawConstants.camIdx + U32(viewIdx)] };
	F32 x{ dot(V4F(vert.pos, 1.0), cam.worldToView.row0) };
	F32 y{ dot(V4F(vert.pos, 1.0), cam.worldToView.row1) };
	F32 z{ dot(V4F(vert.pos, 1.0), cam.worldToView.row2) };
	F32 nearPlane{ 0.05F };
	^outPosition = V4F(x * cam.projXScale + z * cam.projXZBias, -(y * cam.projYScale + z * cam.projYZBias), nearPlane, -z);
	^color = unpack_unorm4x8(vert.color);
	^outPointSize = 4.0;
};

#interface

&V4F color;

#shader fragment

[uniform, set 0, binding 0] &Sampler bilinearSampler;
[uniform, set 0, binding 6] &Image2DSampled[] textures;

[output, location 0] &V4F fragColor;
[output, location 1] &U32 outObjId;

[entrypoint] @[][] frag_main{
	// Multiply by 11.2 (the whitepoint) over 2.0 (the exposure bias) so the debug color looks reasonably bright after tonemapping
	^fragColor = ^color * 5.6;
	^outObjId = 0u;
};