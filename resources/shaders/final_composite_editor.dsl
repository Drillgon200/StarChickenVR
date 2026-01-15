#version 2
#shader compute

#include "tonemap.dsi"

[set 0, binding 0, uniform] &Image2DSampled sceneColor;
[set 0, binding 1, uniform] &ImageU322DSampled sceneObjIds;
[set 0, binding 2, uniform] &Image2DSampled uiImage;
[set 0, binding 3, uniform] &Image2DStorageRGBA8 compositeImage;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	U32 activeObjectId;
	V2U outputDimensions;
} pushConstants;

[entrypoint, localsize 16 16 1] @[][] compute_main{
	V2U fragCoord{ globalInvocationId.xy };
	U32 faceIdx{ globalInvocationId.z };
	V2U outputDim{ pushConstants.outputDimensions };
	if fragCoord.x >= outputDim.x || fragCoord.y >= outputDim.y {
		return;
	};
	U32 activeId{ pushConstants.activeObjectId };
	V4F uiTexel{ (^uiImage)[fragCoord] };
	U32 objIdTexel{ (^sceneObjIds)[fragCoord].x };
	U32 isSelected{ objIdTexel >>> 31u };
	objIdTexel = objIdTexel & 0x7FFFFFFFu;
	V4F sceneTexel{ (^sceneColor)[fragCoord] };
	V3F sceneTonemapped{ uncharted2_filmic(sceneTexel.rgb) };
	sceneTonemapped = sceneTexel.rgb; // debug
	V2U minCoord{ 0u };
	V2U maxCoord{ outputDim };
	U32 id00{ (^sceneObjIds)[clamp(fragCoord + V2U(-1u, -1u), minCoord, maxCoord)].x };
	U32 id10{ (^sceneObjIds)[clamp(fragCoord + V2U(0u, -1u),  minCoord, maxCoord)].x };
	U32 id20{ (^sceneObjIds)[clamp(fragCoord + V2U(1u, -1u),  minCoord, maxCoord)].x };
	U32 id01{ (^sceneObjIds)[clamp(fragCoord + V2U(-1u, 0u),  minCoord, maxCoord)].x };
	U32 id21{ (^sceneObjIds)[clamp(fragCoord + V2U(1u, 0u),   minCoord, maxCoord)].x };
	U32 id02{ (^sceneObjIds)[clamp(fragCoord + V2U(-1u, 1u),  minCoord, maxCoord)].x };
	U32 id12{ (^sceneObjIds)[clamp(fragCoord + V2U(0u, 1u),   minCoord, maxCoord)].x };
	U32 id22{ (^sceneObjIds)[clamp(fragCoord + V2U(1u, 1u),   minCoord, maxCoord)].x };
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
	V4F outlineColor{ 0.953, 0.592, 0.18, min(selectedIntensity * 0.34, 1.0) * F32(isSelected ^ 1u) };
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
		V4F activeColor{ 0.886, 0.341, 0.114, min(activeIntensity * 0.34, 1.0) };
		outlineColor = V4F(mix(outlineColor.rgb, activeColor.rgb, V3F(activeColor.a)), min(outlineColor.a + activeColor.a, 1.0));
	};
	write_image(^compositeImage, fragCoord, V4F(mix(mix(sceneTonemapped, outlineColor.rgb, V3F(outlineColor.a)), uiTexel.rgb, V3F(uiTexel.a)), 1.0));
};