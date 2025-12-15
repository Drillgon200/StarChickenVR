#version 2
#shader compute

[set 0, binding 0, uniform] &Sampler bilinearSampler;
[set 0, binding 2, uniform] &ImageCubeSampled inputImageCube;
[set 0, binding 6, uniform] &ImageU32CubeStorageR32UI[17] outputs;

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

// This generates a much more regular grid than the plastic constant version.
// This is less random, but generates significantly less noisy results than both the plastic quasirandom function and hammersley in my testing
// I think this is because the grid is rotated, so the samples are more spread out in theta/phi when importance sampling
@[V2F r][U32 n, U32 sampleCount] golden_ratio_grid{
	F32 r1{ F32(n) / F32(sampleCount) };
	//F32 r2{ fract(0.5 + F32(n) * 0.61803398875) };
	F32 r2{ F32(n * 0x9E3779B9u + mulhi(n, 0x7F4A7C16u) + 0x80000000u) / 4294967296.0 };
	return V2F(r1, r2);
};

[entrypoint, localsize 16 16 1] @[][] compute_main{
	V2U outputCoord{ globalInvocationId.xy };
	U32 faceIdx{ globalInvocationId.z };
	V2U inputDim{ pushConstants.inputDimensions };
	V2U outputDim{ pushConstants.outputDimensions };
	if outputCoord.x >= outputDim.x || outputCoord.y >= outputDim.y {
		return;
	};

	V2F faceDir{ (V2F(outputCoord) + 0.5) / V2F(outputDim) * 2.0 - 1.0 };
	V3F dir{ 
		if faceIdx == 0u { V3F(1.0, -faceDir.y, -faceDir.x) } // +X
		else if faceIdx == 1u { V3F(-1.0, -faceDir.y, faceDir.x) } // -X
		else if faceIdx == 2u { V3F(faceDir.x, 1.0, faceDir.y) } // +Y
		else if faceIdx == 3u { V3F(faceDir.x, -1.0, -faceDir.y) } // -Y
		else if faceIdx == 4u { V3F(faceDir.x, -faceDir.y, 1.0) } // +Z
		else { V3F(-faceDir.x, -faceDir.y, -1.0) } // -Z
	};
	dir = normalize(dir);
	// https://box2d.org/posts/2014/02/computing-a-basis/
	V3F tan{
		normalize(
			if abs(dir.x) >= 0.57735 { V3F(dir.y, -dir.x, 0.0) }
			else { V3F(0.0, dir.z, -dir.y) }
		)
	};
	V3F bitan{ cross(dir, tan) };

	V3F norm{ dir };
	V3F eye{ dir };
	F32 nDotV{ dot(eye, dir) };

	F32 roughness{ pushConstants.roughness };
	roughness = roughness * roughness;

	U32 sampleCount{ 16384u };
	F32 sampleSolidAngleRatio{ (F32(inputDim.x) * F32(inputDim.y) * 6.0) / F32(sampleCount) };
	F32 mipLevel{ 0.5 * log2(sampleSolidAngleRatio * 4.0) }; // Multiply because the ratio assumes uniform sampling, so I increased the multiplier until banding went away

	V3F color{ 0.0, 0.0, 0.0 };
	for U32 n{ 0 }; n < sampleCount; n = n + 1u {
		V2F randAngles{ golden_ratio_grid(n, sampleCount) };
		F32 t{ 0.5 * 3.1415926535 * randAngles.x };
		F32 p{ 2.0 * 3.1415926535 * randAngles.y };
		V3F sampleDir{ cos(p) * cos(t), sin(t), sin(p) * cos(t) };
		sampleDir = sampleDir.x * tan + sampleDir.y * dir + sampleDir.z * bitan;
		V3F colorSample{ (^inputImageCube)[^bilinearSampler, sampleDir, mipLevel].rgb };
		color = color + colorSample * sin(t) * cos(t);
	};
	color = color * 3.1415926535 / F32(sampleCount);
	write_image((^outputs)[pushConstants.outputIdx], V3U(outputCoord, faceIdx), V4U(pack_E5B9G9R9(color)));
};