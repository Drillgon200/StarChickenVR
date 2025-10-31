#version 2
#shader compute

[set 0, binding 0, uniform] &Sampler bilinearSampler;
// Write out image in VK_FORMAT_E5B9G9R9_UFLOAT_PACK32
// I need to write a BC6H compressor at some point
[set 0, binding 1, uniform] &ImageU32CubeStorageR32UI outputImage;
[set 0, binding 3, uniform] &Image2DSampled inputImageEquirect;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	V2U inputDimensions;
	V2U outputDimensions;
	F32 roughness;
	U32 outputIdx;
	U32 inputIdx;
} pushConstants;

@[V3F color][V3F direction] sample_equirect{
	F32 y{ asin(direction.y) * 0.31830988618 + 0.5 };
	V2F horizontal{ normalize(direction.xz) };
	F32 x{ atan2(horizontal.y, horizontal.x) * 0.31830988618 * 0.5 + 0.5 };
	return (^inputImageEquirect)[^bilinearSampler, V2F(x, y), 0.0F].rgb;
};

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
	V2F faceDir{ (V2F(outputCoord) + 0.5) / V2F(outputDim) * 2.0 - 1.0 };
	V3F dir{ 
		if faceIdx == 0u { V3F(1.0, faceDir.y, -faceDir.x) } // +X
		else if faceIdx == 1u { V3F(-1.0, faceDir.y, faceDir.x) } // -X
		else if faceIdx == 2u { V3F(faceDir.x, -1.0, faceDir.y) } // +Y
		else if faceIdx == 3u { V3F(faceDir.x, 1.0, -faceDir.y) } // -Y
		else if faceIdx == 4u { V3F(faceDir.x, faceDir.y, 1.0) } // +Z
		else { V3F(-faceDir.x, faceDir.y, -1.0) } // -Z
	};
	dir = normalize(dir);
	write_image(^outputImage, V3U(outputCoord, faceIdx), V4U(pack_E5B9G9R9(sample_equirect(dir))));
};