#version 2
#shader compute

#include "tonemap.dsi"

[set 0, binding 0, uniform] &Image2DSampled sceneColor;
[set 0, binding 3, uniform] &Image2DStorageRGBA8 composite;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	U32 activeObjectId;
	V2U outputDimensions;
} pushConstants;

[entrypoint, localsize 16 16 1] @[][] compute_main{
	V2U fragCoord{ globalInvocationId.xy };
	V2U outputDim{ pushConstants.outputDimensions };
	if fragCoord.x >= outputDim.x || fragCoord.y >= outputDim.y {
		return;
	};
	V4F sceneTexel{ (^sceneColor)[fragCoord] };
	V3F sceneTonemapped{ uncharted2_filmic(sceneTexel.rgb) };
	write_image(^composite, fragCoord, V4F(sceneTonemapped, 1.0));
};