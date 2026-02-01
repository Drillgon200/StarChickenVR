#version 2
#include "lib.dsi"
#shader vertex
#extension multiview

[set 0, binding 2, uniform_buffer, restrict, nonwritable] &DrawData drawData;
[push_constant] &WorldDrawPushConstants modelData;

[input, builtin VertexIndex] &I32 inVertexIndex;
[input, builtin ViewIndex] &I32 inViewIndex;

[output, builtin Position] &V4F outPosition;

[entrypoint] @[][] vert_main{
	I32 vertIdx{ modelData.verticesOffset };
	I32 camIdx{ modelData.camIdx };
	V4F pos{ 0.0 };
	V3F norm{ 0.0 };
	V3F tangent{ 0.0 };
	if vertIdx > 0 {
		vertIdx = vertIdx - 1 + ^inVertexIndex;
		pos = V4F(drawData.positions[vertIdx], 1.0);
		norm = drawData.normals[vertIdx];
		tangent = drawData.tangents[vertIdx];
	} else {
		vertIdx = -vertIdx - 1 + ^inVertexIndex;
		pos = V4F(drawData.skinnedPositions[vertIdx], 1.0);
		norm = drawData.skinnedNormals[vertIdx];
		tangent = drawData.skinnedTangents[vertIdx];
	};
	I32 viewIdx{ ^inViewIndex };
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
	V3F transformedTangent{
		dot(tangent, modelMat.row0.xyz),
		dot(tangent, modelMat.row1.xyz),
		dot(tangent, modelMat.row2.xyz)
	};
	Camera cam{ drawData.cams[camIdx + viewIdx] };

	F32 viewX{ dot(worldPos, cam.worldToView.row0) };
	F32 viewY{ dot(worldPos, cam.worldToView.row1) };
	F32 viewZ{ dot(worldPos, cam.worldToView.row2) };
	F32 nearPlane{ 0.05F };
	^outPosition = V4F(viewX * cam.projXScale + viewZ * cam.projXZBias, -(viewY * cam.projYScale + viewZ * cam.projYZBias), nearPlane, -viewZ);
	^passPosition = worldPos.xyz;
	^passNormal = transformedNorm;
	^passTangent = transformedTangent;
	V2F texCoord{ drawData.texcoords[vertIdx] };
	texCoord.y = 1.0 - texCoord.y;
	^passTexCoord = texCoord;
	^passCamPos = cam.position;
	^passObjId = modelData.objId;
	
};

#interface

&V3F passPosition;
&V3F passNormal;
&V3F passTangent;
&V2F passTexCoord;
&V3F passCamPos;
[flat] &U32 passObjId;

#shader fragment

[output, location 0] &V4F outFragColor;
[output, location 1] &U32 outObjId;

[push_constant] &WorldDrawPushConstants modelData;

[uniform, set 0, binding 0] &Sampler bilinearSampler;
[uniform, set 0, binding 1] &Sampler bilinearClampedSampler;
[set 0, binding 2, uniform_buffer, restrict, nonwritable] &DrawData drawData;
[uniform, set 0, binding 3] &ImageCubeSampled specularCubemap;
[uniform, set 0, binding 4] &ImageCubeSampled diffuseCubemap;
[uniform, set 0, binding 5] &Image2DSampled brdfTRLut;
[uniform, set 0, binding 6] &Image2DSampled[] textures;

#include "pbr.dsi"

[entrypoint] @[][] frag_main{
	V3F camPos{ ^passCamPos };
	V3F normal{ normalize(^passNormal) };
	V3F worldPos{ ^passPosition };
	V3F fragToCam{ normalize(camPos - worldPos) };
	V2F texCoord{ ^passTexCoord };

	Material material{ drawData.materials[modelData.materialId] };

	V4F materialColor{ unpack_unorm4x8(material.packedBaseColor) };
	V4F materialARMI{ unpack_unorm4x8(material.packedARMI) };
	if material.baseColorIdx != -1 {
		materialColor = materialColor * (^textures)[material.baseColorIdx][^bilinearSampler, texCoord];
	};
	if material.normalMapIdx != -1 {
		V3F tangent{ normalize(^passTangent) };
		V3F bitangent{ cross(tangent, normal) };
		V3F normalTexel{ (^textures)[material.normalMapIdx][^bilinearSampler, texCoord].xyz * 2.0 - 1.0 };
		normal = normalize(tangent * normalTexel.x + bitangent * normalTexel.y + normal * normalTexel.z);
	};
	if material.armMapIdx != -1 {
		materialARMI = V4F((^textures)[material.armMapIdx][^bilinearSampler, texCoord].xyz, materialARMI.w);
	};
	V3F baseColor{ materialColor.rgb };
	F32 metalness{ materialARMI.z };
	F32 roughness{ materialARMI.y };
	F32 ambientOcclusion{ materialARMI.x };
	F32 iorA{ materialARMI.w * 4.0 + 1.0 }; // IOR passed is normalized, representing a range of 1-5, which encompasses most real materials
	F32 iorB{ 1.0 };
	F32 fresnelReflectionFactor{ (iorA - iorB) / (iorA + iorB) };
	fresnelReflectionFactor = fresnelReflectionFactor * fresnelReflectionFactor;

	V3F diffuseSample{ (^diffuseCubemap)[^bilinearSampler, normal].rgb };
	F32 cubemapLodLevels{ 10.0 };
	V3F specularSample{ (^specularCubemap)[^bilinearSampler, reflect(-fragToCam, normal), roughness * cubemapLodLevels].rgb };
	F32 nDotV{ max(dot(normal, fragToCam), 0.0) };
	V2F brdfLUTSample{ (^brdfTRLut)[^bilinearClampedSampler, V2F(nDotV, roughness), 0.0].xy };
	roughness = roughness * roughness;
	V3F iblLighting{ ibl_fdez_aguera(baseColor, fresnelReflectionFactor, nDotV, diffuseSample, specularSample, brdfLUTSample, roughness, metalness) * ambientOcclusion };
	
	V3F lightDirection{ normalize(V3F(1.0, 1.0, -1.0)) };
	V3F directLighting{ calculate_light_pbr(baseColor, V3F(1.0), fresnelReflectionFactor, lightDirection, fragToCam, normal, roughness, metalness) };

	^outFragColor = V4F(directLighting + iblLighting, 1.0);
	^outObjId = ^passObjId;
};