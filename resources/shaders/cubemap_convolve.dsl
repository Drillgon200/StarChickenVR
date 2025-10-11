#version 2
#shader compute

[set 0, binding 0, uniform] &Image2DStorageRGBA32F inputImage;
[set 0, binding 1, uniform] &ImageCubeStorageRGBA32F outputImage;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	V2U inputDimensions;
	V2U outputDimensions;
} pushConstants;


@[V4F color][V3F direction] sample_equirect{
	U32 y{ U32((direction.y + 1.0) * 0.5 * F32(pushConstants.inputDimensions.y)) };
	V2F horizontal{ normalize(direction.xz) };
	U32 x{ (atan2(horizontal.y, horizontal.x) * 0.31830988618 + 1.0) * 0.5 * F32(pushConstants.inputDimensions.x) };
	V2U coord{ clamp(V2U(x, y), V2U(0u, 0u), pushConstants.inputDimensions - V2U(1u, 1u)) };
	return (^inputImage)[coord];
};

[entrypoint, localsize 16 16 1] @[][] compute_main{
	V2U outputCoord{ globalInvocationId.xy };
	V2U inputDim{ pushConstants.inputDimensions };
	V2U outputDim{ pushConstants.outputDimensions };
	if outputCoord.x >= outputDim.x || outputCoord.y >= outputDim.y {
		return;
	};
	V3F color{ 0.0, 0.0, 0.0 };
	U32 z{ 0u };
	for U32 x{ 0u }; x < outputDim.x; x = x + 1u {
		for U32 y{ 0u }; y < outputDim.y; y = y + 1u {
			//color = color + (^inputImage)[V2U(x, y)].xyz;
			z = z + 3u;
		};
	};
	color = color / F32(outputDim.x * outputDim.y);
	write_image(^outputImage, V3U(outputCoord, globalInvocationId.z), V4F(color, 1.0));
};