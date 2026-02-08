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
	V2U outputDim{ pushData.outputDimensions };
	if fragCoord.x >= outputDim.x || fragCoord.y >= outputDim.y {
		return;
	};
	V4F sceneTexel{ Image2DSampled(pushData.sceneColor)[fragCoord] };
	V3F sceneTonemapped{ uncharted2_filmic(sceneTexel.rgb) };
	write_image(Image2DStorageRGBA8(pushData.compositeOutput), fragCoord, V4F(sceneTonemapped, 1.0));
};