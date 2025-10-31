#version 2
#shader compute

[set 0, binding 0, uniform] &Sampler bilinearSampler;
// Write out image in VK_FORMAT_E5B9G9R9_UFLOAT_PACK32
[set 0, binding 1, uniform] &ImageU32CubeStorageR32UI[17] outputs;
[set 0, binding 4, uniform] &Image2DArraySampled input;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	V2U inputDimensions;
	V2U outputDimensions;
	F32 roughness;
	U32 outputIdx;
	U32 inputIdx;
} pushConstants;

@[U32 packed][V3F x] pack_E5B9G9R9{
	// The 5 bit exponent has no explicit 1 and has a bias of 15
	V3I significand{ I32(f32_significand(x.r) * 512.0), I32(f32_significand(x.g) * 512.0), I32(f32_significand(x.b) * 512.0) };
	V3I exponent{ f32_exponent(x.r), f32_exponent(x.g), f32_exponent(x.b) };
	// We'll just choose the max here, I think it's more important to represent the high numbers accurately
	I32 newExponent{ clamp(max(exponent.x, max(exponent.y, exponent.z)), -15, 16) };
	V3I expDiff{ newExponent - exponent };
	// Try to fit each significand to the chosen exponent.
	// If positive difference, shift down by at most 9 (don't want to accidentally get undefined results with a big difference).
	// If negative difference (only the case when the original exponent was bigger than 5 bits), shift up by 1, causing it to be clamped to 511.
	V3I newSignificand{ min(V3I(511), significand >>> clamp(expDiff, V3I(0), V3I(9)) << clamp(-expDiff, V3I(0), V3I(1))) };
	return U32(newExponent + 15) << 27u |
		U32(newSignificand.b) << 18u |
		U32(newSignificand.g) << 9u |
		U32(newSignificand.r);
};

[entrypoint, localsize 16 16 1] @[][] compute_main{
	V2U outputCoord{ globalInvocationId.xy };
	U32 faceIdx{ globalInvocationId.z };
	V2U outputDim{ pushConstants.outputDimensions };
	if outputCoord.x >= outputDim.x || outputCoord.y >= outputDim.y {
		return;
	};
	V3F texCoord{ (V2F(outputCoord) + 0.5) / V2F(outputDim), F32(faceIdx) };
	V3F color{ (^input)[^bilinearSampler, texCoord, F32(pushConstants.inputIdx)].rgb };

	write_image((^outputs)[pushConstants.outputIdx], V3U(outputCoord, faceIdx), V4U(pack_E5B9G9R9(color)));
};