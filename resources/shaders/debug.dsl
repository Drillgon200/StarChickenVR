#version 2
#shader vertex
#extension multiview

struct M4x3F {
	V4F row0;
	V4F row1;
	V4F row2;
};

struct Vertex {
	V3F pos;
	U32 color;
};

struct Camera {
	M4x3F worldToView;
	F32 projXScale;
	F32 projXZBias;
	F32 projYScale;
	F32 projYZBias;
	V3F position;
	V3F direction;
};

struct Material {
	U32 colorTexIdx;
	U32 normalTexIdx;
	U32 roughnessTexIdx;
	F32 ior;
};

[set 0, binding 1, uniform_buffer, restrict, nonwritable, block] &struct {
	V2F screenDimensions;
	&V4F uiClipBoxes;
	V2U pUIVertices;
	&M4x3F matrices;
	&V3F positions;
	&V2F texcoords;
	&V3F normals;
	&V3F tangents;
	&U32 boneIndicesAndWeights;
	&V3F skinnedPositions;
	&V3F skinnedNormals;
	&V3F skinnedTangents;
	&Material materials;
	&Camera cams;
} drawData;
[push_constant, block] &struct {
	U32 transformIdx;
	I32 verticesOffset;
} modelData;

[input, builtin VertexIndex] &I32 inVertexIndex;
[input, builtin ViewIndex] &I32 inViewIndex;
[output, builtin Position] &V4F outPosition;
[output, builtin PointSize] &F32 outPointSize;

[entrypoint] @[][] vert_main {
	&Vertex vertexBuffer{ (&Vertex)(v2u_u64_add(drawData.pUIVertices, U32(modelData.verticesOffset))) };
	Vertex vert{ vertexBuffer[^inVertexIndex] };

	I32 viewIdx{ ^inViewIndex };
	Camera cam{ drawData.cams[viewIdx] };
	M4x3F viewProj{ drawData.matrices[viewIdx + 1] };
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
[uniform, set 0, binding 2] &Image2DSampled[] textures;

[output, location 0] &V4F fragColor;

[entrypoint] @[][] frag_main{
	^fragColor = ^color;
};