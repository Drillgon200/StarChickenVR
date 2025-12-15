#version 2
#shader compute

[set 0, binding 0, uniform] &Sampler bilinearSampler;
[set 0, binding 2, uniform] &ImageCubeSampled inputImageCube;
[set 0, binding 5, uniform] &ImageU32CubeStorageR32UI[17] outputs;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	V2U inputDimensions;
	V2U outputDimensions;
	F32 roughness;
	U32 outputIdx;
	U32 inputIdx;
} pushConstants;

@[V2F r][U32 i, U32 n] hammersley{
    U32 bits{ i };
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >>> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >>> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >>> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >>> 8u);
    bits = (bits << 16u) | (bits >>> 16u);
    return V2F(F32(i) / F32(n), F32(bits) * 2.3283064365386963e-10);
};

@[V2F r][U32 i, U32 n] uniform_grid{
	F32 n2{ sqrt(F32(n)) };
    return V2F(F32(i) % n2 / n2, F32(i) / F32(n));
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

@[F32 prob][F32 nDotH, F32 roughness] normal_distribution_trowbridge_reitz{
	// https://jcgt.org/published/0003/02/03/paper.pdf
	F32 r2{ roughness * roughness };
	F32 nDotH2{ nDotH * nDotH };
	F32 denom{ nDotH2 * (r2 - 1.0) + 1.0 };
	return r2 / (denom * denom * 3.1415926535);
};

@[V2F r][U32 n] quasirandom2d{
	/*
	Having a low discrepancy quasirandom sequence is helpful because it samples more evenly
	I'm using this golden ratio (or actually "plastic constant", 1.324717957244746, in the 2d case) based sequence from this article, where each random V2F is (0.5 + n * (1 / plastic), 0.5 + n * (1 / plastic^2))
	https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
	F32 multiplication precision is low enough to cause issues with high sample counts, so the actual calculation is done by muliplying n with the relevant constant times 2^64, then dividing by 2^64 to get the 0-1 random
	64 bits is overkill, but it's cheap, so I might as well use it.
	*/
	//F32 r1{ fract(0.5 + F32(n) * 0.75487766624) };
	//F32 r2{ fract(0.5 + F32(n) * 0.56984029099) };
	F32 r1{ F32(n * 0xC13FA9A9u + mulhi(n, 0x02A6328Fu) + 0x80000000u) / 4294967296.0 };
	F32 r2{ F32(n * 0x91E10DA5u + mulhi(n, 0xC79E7B1Cu) + 0x80000000u) / 4294967296.0 };
	return V2F(r1, r2);
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


@[V3F dir][U32 n, U32 sampleCount, F32 roughness] importance_sample_trowbridge_reitz{
	/*
	Thanks to this post for an explanation of importance sampling.
	https://patapom.com/blog/Math/ImportanceSampling/
	I'll redo the math here to make sure I understand everything
	We want to bend our samples toward the NDF because otherwise random samples would mostly miss the specular peaks we care about at low roughness.
	int[0, 2pi] int[0,pi/2] NDF(t, r) * cos(t) * sin(t) dt dp == 1
	Where t is the angle between the halfway vector we're integrating and the surface normal.
	The extra cos(t) is because the NDF is defined in terms of microfacet area and we want to project that onto the normal.
	See https://www.reedbeta.com/blog/hows-the-ndf-really-defined/
	We need the CDF of the function, which is the probability that a randomly selected value in the function area will be <= the input
	For an NDF, we can take the integral from 0 to X to find it
	int (r^2 / (pi * ((r^2 - 1)cos(t)^2 + 1)^2))cos(t)sin(t) dt
	u = cos(t)
	du = -sin(t)dt
	dt = (-1/sin(t))du
	int -u(r^2 / (pi * ((r^2 - 1)u^2 + 1)^2)) du
	v = u^2
	dv = 2u du
	du = dv / 2u
	int -0.5(r^2 / (pi * ((r^2 - 1)v + 1)^2)) dv
	(-0.5r^2 / pi) int 1 / ((r^2 - 1)v + 1)^2 dv
	w = (r^2 - 1)v + 1
	dw = (r^2 - 1) dv
	dv = dw / (r^2 - 1)
	(-0.5r^2 / ((r^2 - 1)pi)) int 1 / w^2 dw
	(-0.5r^2 / ((r^2 - 1)pi)) * (-1/w)
	0.5r^2 / (w(r^2-1)pi)
	0.5r^2 / (((r^2 - 1)v + 1)(r^2-1)pi)
	0.5r^2 / (((r^2 - 1)u^2 + 1)(r^2-1)pi)
	0.5r^2 / (((r^2 - 1)cos(t)^2 + 1)(r^2-1)pi)

	This will still be in the outer integral from 0-2pi. Since the NDF is isotropic, there is no dependency on p, and we can just multiply by 2pi
	The final CDF is this
	r^2 / ((r^2 - 1)cos(t)^2 + 1)(r^2-1) - r^2 / ((r^2 - 1)cos(0)^2 + 1)(r^2-1)
	r^2 / ((r^2 - 1)cos(t)^2 + 1)(r^2-1) - r^2 / ((r^2 - 1) + 1)(r^2-1)
	r^2 / ((r^2 - 1)cos(t)^2 + 1)(r^2-1) - r^2 / (r^2)(r^2-1)
	r^2 / ((r^2 - 1)cos(t)^2 + 1)(r^2-1) - 1 / (r^2-1)
	r^2 / ((r^2 - 1)cos(t)^2 + 1)(r^2-1) - ((r^2 - 1)cos(t)^2 + 1) / ((r^2 - 1)cos(t)^2 + 1)(r^2-1)
	(r^2 - (r^2 - 1)cos(t)^2 - 1) / ((r^2 - 1)cos(t)^2 + 1)(r^2-1)
	(r^2 - 1)(1 - cos(t)^2) / ((r^2 - 1)cos(t)^2 + 1)(r^2-1)
	(1 - cos(t)^2) / ((r^2 - 1)cos(t)^2 + 1)

	// The CDF answers the question "given t, what is the probability a random point is <= t"
	// We want the inverse question, "given the probability, what's the t where a random point is <= t with that probability"
	p = (1 - cos(t)^2) / ((r^2 - 1)cos(t)^2 + 1)
	((r^2 - 1)cos(t)^2 + 1)p = 1 - cos(t)^2
	(cos(t)^2)((r^2 - 1)p + 1) + p = 1
	cos(t)^2 = (1 - p) / ((r^2 - 1)p + 1)
	cos(t) = sqrt((1 - p) / ((r^2 - 1)p + 1))

	The rotation phi can just be random because the function is isotropic
	*/
	
	V2F r{ golden_ratio_grid(n, sampleCount) };
	//r = uniform_grid(n, sampleCount);
	//r = quasirandom2d(n);
	//r = hammersley(n, sampleCount);
	F32 cost{ sqrt((1.0 - r.x) / ((roughness * roughness - 1.0) * r.x + 1.0)) };
	F32 sint{ sqrt(1.0 - cost * cost) };
	F32 p{ 2.0 * 3.1415926535 * r.y };
	return V3F(cos(p) * sint, cost, sin(p) * sint);
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

	F32 roughness{ pushConstants.roughness };
	roughness = roughness * roughness;

	// Not accurate for every texel, but the mip level selection doesn't need to be accurate anyway.
	F32 rcpTexelSolidAngle{ (F32(inputDim.x) * F32(inputDim.y) * 6.0) / (4.0 * 3.1415926535) };

	V3F color{ 0.0, 0.0, 0.0 };
	U32 sampleCount{ 16384u };
	for U32 n{ 0 }; n < sampleCount; n = n + 1u {
		V3F half{ importance_sample_trowbridge_reitz(n, sampleCount, roughness) };
		half = half.x * tan + half.y * dir + half.z * bitan;
		V3F toLight{ normalize(reflect(-eye, half)) };
		F32 nDotL{ dot(toLight, norm) };
		if nDotL > 0.0 {
			F32 nDotH{ dot(half, norm) };
			F32 vDotH{ dot(eye, half) };
			/*
			Divide by nDotH because the PDF contained a factor of nDotH so it could integrate to 1, and we divided out the PDF (the rest of the PDF canceled with the NDF)
			No nDotV divide because N and V are the same in this approximation
			Our PDF is with respect to the halfway vector, but this integral is with respect to the light vector
			We can divide out the solid angle for the half integral and multiply by the solid angle for the light integral to convert a PDF division with respect to half to a PDF division with respect to light
			These angles are with respect to h, not the normal. Theta is the angle from the eye to the light or half vector, respectively.
			(sin(tl) dtl dpl) / (sin(th) dth dph)
			The phis are the same, only differing by a constant offset, so they cancel
			(sin(tl) dtl) / (sin(th) dth)
			Since l is reflected from the eye vector around h, tl = 2th
			(sin(2th) 2dth) / (sin(th) dth)
			(sin(2th) dth) / sin(th)
			2cos(2th) / sin(th)
			4sin(th)cos(th) / sin(th)
			4cos(th)
			The 4 cancels with the 4 from the BRDF, and we multiply by vDotH
			Thanks to "Physically Based Rendering: From Theory to Implementation" Third Edition pages 812-813 for clearing up that confusion
			*/
			// Mipmap weighting based on GPU Gems 3
			// https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-20-gpu-based-importance-sampling
			F32 p{ normal_distribution_trowbridge_reitz(nDotH, roughness) * nDotH / (4.0 * vDotH) };
			F32 sampleSolidAngle{ 1.0 / max(p * F32(sampleCount), 0.0001) };
			F32 mipLevel{ 0.5 * log2(sampleSolidAngle * rcpTexelSolidAngle) };
			color = color + (^inputImageCube)[^bilinearSampler, toLight, mipLevel].rgb * (vDotH / nDotH);
		};
	};
	color = color / F32(sampleCount);
	write_image((^outputs)[pushConstants.outputIdx], V3U(outputCoord, faceIdx), V4U(pack_E5B9G9R9(color)));

	// This code is just for debug visualization, don't mind the potential data races
	/*
	write_image((^outputs)[pushConstants.outputIdx], V3U(outputCoord, faceIdx), V4U(0u));
	for U32 n{ 0 }; n < sampleCount; n = n + 1u {
		V2F coord{ golden_ratio_grid(n, sampleCount) };
		coord = quasirandom2d(n);
		coord = hammersley(n, sampleCount);
		coord = uniform_grid(n, sampleCount);
		write_image((^outputs)[pushConstants.outputIdx], V3U(U32(coord.x * F32(outputDim.x)), U32(coord.y * F32(outputDim.y)), faceIdx), V4U(pack_E5B9G9R9(V3F(1.0))));
	};
	*/
	
	
};