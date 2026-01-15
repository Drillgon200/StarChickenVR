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

struct M4x3F {
	V4F row0;
	V4F row1;
	V4F row2;
};

[output, location 0] &V4F outFragColor;
[output, location 1] &U32 outObjId;
[input, location 0] &V2F passPos;
[input, location 1, flat] &I32 passViewIndex;

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

[uniform, set 0, binding 0] &Sampler bilinearSampler;
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
[uniform, set 0, binding 2] &ImageCubeSampled backgroundCube;
[uniform, set 0, binding 3] &ImageCubeSampled diffuseCube;

//TODO do some actual research into tonemappers
// https://64.github.io/tonemapping/
@[V3F mapped][V3F x] uncharted2_tonemap_partial{
    F32 A{ 0.15 };
    F32 B{ 0.50 };
    F32 C{ 0.10 };
    F32 D{ 0.20 };
    F32 E{ 0.02 };
    F32 F{ 0.30 };
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
};
@[V3F mapped][V3F v] uncharted2_filmic{
    F32 exposureBias{ 2.0 };
    V3F whitePoint{ 11.2 };
    return uncharted2_tonemap_partial(v * exposureBias) / uncharted2_tonemap_partial(whitePoint);
};

[entrypoint] @[][] frag_main{
	I32 viewIdx{ ^passViewIndex };
	Camera cam{ drawData.cams[viewIdx] };
	V2F pos{ ^passPos };
	V3F worldDirection{ -(pos.x - cam.projXZBias) / cam.projXScale, (pos.y - cam.projYZBias) / cam.projYScale, 1.0 };
	worldDirection = normalize(worldDirection);
	worldDirection = worldDirection.x * cam.worldToView.row0.xyz + worldDirection.y * cam.worldToView.row1.xyz + worldDirection.z * cam.worldToView.row2.xyz;
	worldDirection = V3F(worldDirection.x, -worldDirection.y, worldDirection.z);

	//^outFragColor = V4F(uncharted2_filmic((^backgroundCube)[^bilinearSampler, worldDirection, 1.0].rgb), 1.0);
	^outFragColor = V4F(uncharted2_filmic((^diffuseCube)[^bilinearSampler, worldDirection, 0.0].rgb), 1.0);
	^outObjId = 0u;
};