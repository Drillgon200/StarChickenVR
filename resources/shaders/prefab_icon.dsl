#version 2
#include "lib.dsi"
#shader vertex

[set 0, binding 2, uniform_buffer, restrict, nonwritable] &DrawData drawData;
[push_constant] &WorldDrawPushConstants modelData;

[input, builtin VertexIndex] &I32 inVertexIndex;

[output, builtin Position] &V4F outPosition;

[entrypoint] @[][] vert_main{
	I32 vertIdx{ modelData.verticesOffset + ^inVertexIndex };
	V4F pos{ drawData.positions[vertIdx], 1.0 };
	V3F norm{ drawData.normals[vertIdx] };
	M4x3F modelMat{ drawData.matrices[modelData.transformIdx] };
	V4F worldPos{
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
	Camera cam{ drawData.cams[0] };

	F32 viewX{ dot(worldPos, cam.worldToView.row0) };
	F32 viewY{ dot(worldPos, cam.worldToView.row1) };
	F32 viewZ{ dot(worldPos, cam.worldToView.row2) };
	F32 nearPlane{ 0.05F };
	^outPosition = V4F(viewX * cam.projXScale + viewZ * cam.projXZBias, -(viewY * cam.projYScale + viewZ * cam.projYZBias), nearPlane, -viewZ);
	V2F texCoord{ drawData.texcoords[vertIdx] };
	texCoord.y = 1.0 - texCoord.y;
	^passTexCoord = texCoord;
	^passNormal = transformedNorm;
};

#interface

&V2F passTexCoord;
&V3F passNormal;

#shader fragment

[output, location 0] &V4F outFragColor;

[push_constant] &WorldDrawPushConstants modelData;

[uniform, set 0, binding 0] &Sampler bilinearSampler;
[set 0, binding 2, uniform_buffer, restrict, nonwritable] &DrawData drawData;
[uniform, set 0, binding 6] &Image2DSampled[] textures;

[entrypoint] @[][] frag_main{
	V2F texCoord{ ^passTexCoord };
	Material material{ drawData.materials[modelData.materialId] };
	V4F materialColor{ unpack_unorm4x8(material.packedBaseColor) };
	if material.baseColorIdx != -1 {
		materialColor = materialColor * (^textures)[material.baseColorIdx][^bilinearSampler, texCoord];
	};
	V3F baseColor{ materialColor.rgb };
	V3F lightDir{ normalize(V3F(1.0, 1.0, 1.0)) };
	F32 lighting{ dot(lightDir, normalize(^passNormal)) * 0.5 + 0.5 };
	^outFragColor = V4F(baseColor * lighting, 1.0);
};