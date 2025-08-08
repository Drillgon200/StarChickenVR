#version 2
#shader vertex
#extension multiview

struct M4x3F {
	V4F row0;
	V4F row1;
	V4F row2;
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
} drawData;
[push_constant, block] &struct {
	U32 transformIdx;
	I32 verticesOffset;
} modelData;

[input, builtin VertexIndex] &I32 inVertexIndex;
[input, builtin ViewIndex] &I32 inViewIndex;

[output, builtin Position] &V4F outPosition;
[output, location 0] &V3F passPos;
[output, location 1] &V3F passNormal;

[entrypoint] @[][] vert_main{
	I32 vertIdx{ modelData.verticesOffset };
	V4F pos{ 0.0 };
	V3F norm{ 0.0 };
	if vertIdx > 0 {
		vertIdx = vertIdx - 1 + ^inVertexIndex;
		pos = V4F(drawData.positions[vertIdx], 1.0);
		norm = drawData.normals[vertIdx];
	} else {
		vertIdx = -vertIdx - 1 + ^inVertexIndex;
		pos = V4F(drawData.skinnedPositions[vertIdx], 1.0);
		norm = drawData.skinnedNormals[vertIdx];
	};
	I32 viewIdx{ ^inViewIndex };
	M4x3F modelMat{ drawData.matrices[modelData.transformIdx] };
	V4F transformedPos{
		dot(pos, modelMat.row0),
		dot(pos, modelMat.row1),
		dot(pos, modelMat.row2),
		1.0F
	};
	V3F transformedNorm{
		dot(norm, modelMat.row0.xyz),
		dot(norm, modelMat.row1.xyz),
		dot(norm, modelMat.row2.xyz)
	};
	M4x3F viewProj{ drawData.matrices[viewIdx + 1] };
	F32 x{ dot(transformedPos, viewProj.row0) };
	F32 y{ dot(transformedPos, viewProj.row1) };
	F32 z{ 0.05F }; // Near plane
	// row2 is actually row3 in  this case, since the matrix is a ProjectiveTransformMatrix
	F32 w{ dot(transformedPos, viewProj.row2) };
	^outPosition = V4F(x, -y, z, w);
	^passPos = transformedPos.xyz;
	^passNormal = transformedNorm;
};

//---------------------------------------------------------//
#shader fragment

[output, location 0] &V4F outFragColor;
[input, location 0] &V3F passPosition;
[input, location 1] &V3F passNormal;

[entrypoint] @[][] frag_main{
	//^outFragColor = V4F(1.0, 0.0, 0.0, 1.0);
	V3F lightDir{ 0.57735026919F, 0.57735026919F, 0.57735026919F };
	^outFragColor = V4F(V3F(dot(normalize(^passNormal), lightDir) * 0.5 + 0.5), 1.0);
};