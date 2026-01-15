#version 2
#shader vertex
#extension multiview

#include "lib.dsi"

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
[push_constant] &WorldDrawPushConstants modelData;

[input, builtin VertexIndex] &I32 inVertexIndex;
[input, builtin ViewIndex] &I32 inViewIndex;

[output, builtin Position] &V4F outPosition;
[output, location 0] &V3F passPos;
[output, location 1] &V3F passNormal;
[output, location 2] &V3F passCamPos;
[output, location 3, flat] &U32 passObjId;

[entrypoint] @[][] vert_main{
	I32 vertIdx{ modelData.verticesOffset };
	I32 camIdx{ modelData.camIdx };
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
	Camera cam{ drawData.cams[camIdx + viewIdx] };

	F32 x{ dot(transformedPos, cam.worldToView.row0) };
	F32 y{ dot(transformedPos, cam.worldToView.row1) };
	F32 z{ dot(transformedPos, cam.worldToView.row2) };
	F32 nearPlane{ 0.05F };
	^outPosition = V4F(x * cam.projXScale + z * cam.projXZBias, -(y * cam.projYScale + z * cam.projYZBias), nearPlane, -z);
	^passPos = transformedPos.xyz;
	^passNormal = transformedNorm;
	^passCamPos = cam.position;
	^passObjId = modelData.objId;
};

//---------------------------------------------------------//
#shader fragment

#include "tonemap.dsi"

[output, location 0] &V4F outFragColor;
[output, location 1] &U32 outObjId;
[input, location 0] &V3F passPosition;
[input, location 1] &V3F passNormal;
[input, location 2] &V3F passCamPos;
[input, location 3, flat] &U32 passObjId;

[uniform, set 0, binding 0] &Sampler bilinearSampler;
[uniform, set 0, binding 2] &ImageCubeSampled specularCubemap;
[uniform, set 0, binding 3] &ImageCubeSampled diffuseCubemap;
[uniform, set 0, binding 4] &Image2DSampled brdfTRLut;
[uniform, set 0, binding 5] &Image2DSampled[] textures;

// The F82 fresnel function more accurately represents the real fresnel effect on metals
@[V3F fresnel][V3F baseColor, V3F specularColor, F32 lDotH] fresnel_f82{
	// u = lDotH;
	// uBar = 1.0 / 7.0;
	F32 f82Factor{ 17.6513846022 }; // 1 / (uBar * (1 - uBar)^6)
	V3F fresnelSchlickOfUBar{ baseColor + (1.0 - baseColor) * 0.46266436603 }; // F0 + (1 - F0)(1 - uBar)^5
	F32 invLDotH{ 1.0 - lDotH };
	F32 invLDotH2{ invLDotH * invLDotH };
	F32 invLDotH5{ invLDotH2 * invLDotH2 * invLDotH };
	F32 invLDotH6{ invLDotH5 * invLDotH };
	V3F fresnelShlickOfU{ baseColor + (1.0 - baseColor) * invLDotH5 };
	return fresnelShlickOfU - lDotH * invLDotH6 * f82Factor * (fresnelSchlickOfUBar - specularColor * fresnelSchlickOfUBar);
};
@[F32 percentage][F32 iorA, F32 iorB, F32 lDotH] fresnel_schlick{
	F32 reflectionFactor{ (iorA - iorB) / (iorA + iorB) };
	reflectionFactor = reflectionFactor * reflectionFactor;
	F32 invLDotH{ 1.0 - lDotH };
	F32 invLDotH2{ invLDotH * invLDotH };
	F32 invLDotH5{ invLDotH2 * invLDotH2 * invLDotH };
	return reflectionFactor + (1.0 - reflectionFactor) * invLDotH5;
};

// "EON: A practical energy-preserving rough diffuse BRDF"
// https://arxiv.org/pdf/2410.18026
@[F32 g][F32 cosAngle] directional_albedo_g_approx_oren_nayar{
	F32 cosAngleInv{ 1.0 - cosAngle };
	// A polynomial approximation on 0 to pi/2 for (sin(x) * (x - sin(x) * cos(x)) + (2/3)(tan(x)(1-sin(x)^3) - sin(x))) / pi
	return cosAngleInv * (0.0571085289 + cosAngleInv * (0.491881867 + cosAngleInv * (-0.332181442 + 0.0714429953 * cosAngleInv)));
};
@[V3F eon][V3F diffuseColor, F32 vDotL, F32 nDotL, F32 nDotV, F32 roughness] diffuse_energy_preserving_oren_nayar{
	F32 PI{ 3.14159265 };
	F32 PI_RCP{ 0.31830989 };
	F32 s{ vDotL - nDotL * nDotV };
	F32 tRcp{ if s <= 0.0 { 1.0 } else { 1.0 / max(nDotL, nDotV) } };
	F32 A{ 1.0 / (1.0 + (0.5 - 2.0 / (3.0 * PI)) * roughness) };
	F32 B{ roughness * A };

	F32 albedoAverage{ A + (2.0 / 3.0 - 28.0 / (15.0 * PI)) * B };
	V3F colorMultiscattering{ diffuseColor * diffuseColor * PI_RCP * (albedoAverage / (1.0 - diffuseColor * (1.0 - albedoAverage))) };
	F32 directionalAlbedo1{ A + B * directional_albedo_g_approx_oren_nayar(nDotL) };
	F32 directionalAlbedo2{ A + B * directional_albedo_g_approx_oren_nayar(nDotV) };

	V3F diffuseOrenNayar{ diffuseColor * PI_RCP * (A + B * s * tRcp) };
	V3F energyCompensation{ colorMultiscattering * ((1.0 - directionalAlbedo1) * (1.0 - directionalAlbedo2) / (1.0 - albedoAverage)) };
	return diffuseOrenNayar + energyCompensation;
};

[entrypoint] @[][] frag_main{
	V3F camPos{ ^passCamPos };
	V3F normal{ normalize(^passNormal) };
	V3F worldPos{ ^passPosition };
	V3F fragToCam{ normalize(camPos - worldPos) };
	F32 nDotV{ max(0.0, dot(normal, fragToCam)) };

	V3F baseColor{ 1.0, 1.0, 1.0 };
	F32 metalness{ 0.0 };
	F32 roughness{ 0.8 };
	F32 ambientOcclusion{ 1.0 };
	F32 iorA{ 1.5 };
	F32 iorB{ 1.0 };
	F32 fresnelReflectionFactor{ (iorA - iorB) / (iorA + iorB) };
	fresnelReflectionFactor = fresnelReflectionFactor * fresnelReflectionFactor;

	
	V3F diffuseSample{ (^diffuseCubemap)[^bilinearSampler, normal].rgb };
	F32 cubemapLodLevels{ 10.0 };
	V3F specularSample{ (^specularCubemap)[^bilinearSampler, reflect(-fragToCam, normal), roughness * cubemapLodLevels].rgb };
	V2F brdfLUTSample{ (^brdfTRLut)[^bilinearSampler, V2F(nDotV, roughness), 0.0].xy };

	// "A Multiple-scattering Microfacet Model for Real-time Image-based Lighting"
	// https://www.jcgt.org/published/0008/01/03/paper.pdf
	V3F f0{ mix(V3F(fresnelReflectionFactor), baseColor, V3F(metalness)) };
	F32 invNDotV{ 1.0 - nDotV };
	F32 invNDotV2{ invNDotV * invNDotV };
	F32 invNDotV5{ invNDotV2 * invNDotV2 * invNDotV };
	V3F roughnessDependentFresnel{ f0 + (max(f0, V3F(1.0 - roughness)) - f0) * invNDotV5 };
	V3F fresnelEnergySingleScattering{ brdfLUTSample.x + brdfLUTSample.y * roughnessDependentFresnel };
	F32 energySingleScattering{ brdfLUTSample.x + brdfLUTSample.y };
	F32 energyMultipleScattering{ 1.0 - energySingleScattering }; // Ensures energy conservation in the white furnace test
	V3F fresnelAvg{ f0 + (1.0 - f0) * (1.0 / 21.0) };
	V3F fresnelMultiscattering{ fresnelAvg * (fresnelEnergySingleScattering / (1.0 - energyMultipleScattering * fresnelAvg)) };
	V3F fresnelEnergyMultiScattering{ fresnelMultiscattering * energyMultipleScattering };
	V3F energyDielectricSingleScattering{ 1.0 - (fresnelEnergySingleScattering + fresnelEnergyMultiScattering) };
	V3F diffuseSingleScattering{ baseColor * energyDielectricSingleScattering };

	V3F finalColor{ (fresnelEnergySingleScattering * specularSample + (diffuseSingleScattering + fresnelEnergyMultiScattering) * diffuseSample) * ambientOcclusion };
	finalColor = uncharted2_filmic(finalColor);

	finalColor = V3F(0.5);

	^outFragColor = V4F(finalColor, 1.0);
	^outObjId = ^passObjId;
};