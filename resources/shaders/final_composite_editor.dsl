#version 2
#shader compute

#include "tonemap.dsi"

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	U32 sceneColor;
	U32 sceneIds;
	U32 uiColor;
	U32 compositeOutput;
	U32 activeObjectId;
	V2U outputDimensions;
} pushData;

[entrypoint, localsize 16 16 1] @[][] compute_main{
	V2U fragCoord{ globalInvocationId.xy };
	U32 faceIdx{ globalInvocationId.z };
	V2U outputDim{ pushData.outputDimensions };
	if fragCoord.x >= outputDim.x || fragCoord.y >= outputDim.y {
		return;
	};
	U32 activeId{ pushData.activeObjectId };
	V4F uiTexel{ Image2DSampled(pushData.uiColor)[fragCoord] };
	ImageU322DSampled sceneObjIds{ pushData.sceneIds };
	U32 objIdTexel{ sceneObjIds[fragCoord].x };
	U32 isSelected{ objIdTexel >>> 31u };
	objIdTexel = objIdTexel & 0x7FFFFFFFu;
	V4F sceneTexel{ Image2DSampled(pushData.sceneColor)[fragCoord] };
	V3F sceneTonemapped{ uncharted2_filmic(sceneTexel.rgb) };
	//sceneTonemapped = sceneTexel.rgb;
	V2U minCoord{ 0u };
	V2U maxCoord{ outputDim };
	U32 id00{ sceneObjIds[clamp(fragCoord + V2U(-1u, -1u), minCoord, maxCoord)].x };
	U32 id10{ sceneObjIds[clamp(fragCoord + V2U(0u, -1u),  minCoord, maxCoord)].x };
	U32 id20{ sceneObjIds[clamp(fragCoord + V2U(1u, -1u),  minCoord, maxCoord)].x };
	U32 id01{ sceneObjIds[clamp(fragCoord + V2U(-1u, 0u),  minCoord, maxCoord)].x };
	U32 id21{ sceneObjIds[clamp(fragCoord + V2U(1u, 0u),   minCoord, maxCoord)].x };
	U32 id02{ sceneObjIds[clamp(fragCoord + V2U(-1u, 1u),  minCoord, maxCoord)].x };
	U32 id12{ sceneObjIds[clamp(fragCoord + V2U(0u, 1u),   minCoord, maxCoord)].x };
	U32 id22{ sceneObjIds[clamp(fragCoord + V2U(1u, 1u),   minCoord, maxCoord)].x };
	F32 selectedIntensity{
		F32(id00 >>> 31u) +
		F32(id10 >>> 31u) +
		F32(id20 >>> 31u) +
		F32(id01 >>> 31u) +
		F32(id21 >>> 31u) +
		F32(id02 >>> 31u) +
		F32(id12 >>> 31u) +
		F32(id22 >>> 31u)
	};
	V4F outlineColor{ 0.886, 0.341, 0.114, min(selectedIntensity * 0.34, 1.0) * F32(isSelected ^ 1u) };
	if activeId != 0u && objIdTexel != activeId {
		F32 activeIntensity{
			select(id00 & 0x7FFFFFFFu == activeId, 1.0, 0.0) +
			select(id10 & 0x7FFFFFFFu == activeId, 1.0, 0.0) +
			select(id20 & 0x7FFFFFFFu == activeId, 1.0, 0.0) +
			select(id01 & 0x7FFFFFFFu == activeId, 1.0, 0.0) +
			select(id21 & 0x7FFFFFFFu == activeId, 1.0, 0.0) +
			select(id02 & 0x7FFFFFFFu == activeId, 1.0, 0.0) +
			select(id12 & 0x7FFFFFFFu == activeId, 1.0, 0.0) +
			select(id22 & 0x7FFFFFFFu == activeId, 1.0, 0.0)
		};
		V4F activeColor{ 0.953, 0.592, 0.18, min(activeIntensity * 0.34, 1.0) };
		outlineColor = V4F(mix(outlineColor.rgb, activeColor.rgb, V3F(activeColor.a)), min(outlineColor.a + activeColor.a, 1.0));
	};
	V3F outColor{ mix(mix(sceneTonemapped, outlineColor.rgb, V3F(outlineColor.a)), uiTexel.rgb, V3F(uiTexel.a)) };
	write_image(Image2DStorageRGBA8(pushData.compositeOutput), fragCoord, V4F(to_srgb(clamp(outColor, V3F(0.0), V3F(1.0))), 1.0));
};