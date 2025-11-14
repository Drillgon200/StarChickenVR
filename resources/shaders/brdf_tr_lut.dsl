#version 2
#shader compute

[set 0, binding 7, uniform] &ImageCubeStorageRG8 outputTR;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	V2U inputDimensions;
	V2U outputDimensions;
	// This is only used for the cubemap convolution part
	F32 roughness;
} pushConstants;

@[F32 maskShadow][F32 nDotL, F32 nDotV, F32 roughness] masking_shadowing_trowbridge_reitz{
	// https://jcgt.org/published/0003/02/03/paper.pdf
	// "Physically Based Rendering: From Theory to Implementation" third edition, page 541
	F32 a2{ roughness * roughness };
	F32 cosIn2{ nDotL * nDotL };
	F32 maskingShadowingIn{ 0.5 * (sqrt(1.0 + a2 * (1.0 - cosIn2) / cosIn2) - 1.0) };
	F32 cosOut2{ nDotV * nDotV };
	F32 maskingShadowingOut{ 0.5 * (sqrt(1.0 + a2 * (1.0 - cosOut2) / cosOut2) - 1.0) };
	// Height correlated combination 
	return 1.0 / (1.0 + maskingShadowingIn + maskingShadowingOut);
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
	F32 r1{ F32(n * 0xC13FA9A9u + mulhi(n, 0x02A6328Fu) + 0x80000000u) / (4294967296.0) };
	F32 r2{ F32(n * 0x91E10DA5u + mulhi(n, 0xC79E7B1Cu) + 0x80000000u) / (4294967296.0) };
	return V2F(r1, r2);
};

@[V3F dir][U32 n, F32 roughness] importance_sample_trowbridge_reitz{
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
	
	V2F r{ quasirandom2d(n) };
	F32 cost{ sqrt((1.0 - r.x) / ((roughness * roughness - 1.0) * r.x + 1.0)) };
	F32 sint{ sqrt(1.0 - cost * cost) };
	F32 p{ 2.0 * 3.1415926535 * r.y };
	return V3F(cos(p) * sint, cost, sin(p) * sint);
};

[entrypoint, localsize 16 16 1] @[][] compute_main{
	V2U outputCoord{ globalInvocationId.xy };
	U32 faceIdx{ globalInvocationId.z };
	V2U outputDim{ pushConstants.outputDimensions };
	if outputCoord.x >= outputDim.x || outputCoord.y >= outputDim.y {
		return;
	};

	// This is the BRDF part of the split sum approximation (assuming integral[vary normal and view and roughness](L * BRDF) ~= integral[vary normal and roughness, assume E=N](L * BRDF) * integral[vary nDotV and roughness, N=constant](BRDF))
	/*
	For dielectric trowbridge reitz, we're trying to integrate this functinon
	(r + (1 - r) * (1 - LdotH)^5) * (R^2 / (NdotH^2 * (R^2 - 1) + 1)^2) * (1.0 / (1.0 + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotL^2)/NdotL^2) - 1) + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotV^2)/NdotV^2) - 1))) / (NdotL * NdotV * 4pi)
	Where
	L = to light
	V = to eye
	N = normal
	H = normalize(L + E)
	R = roughness
	r = ((iorA - iorB) / (iorA + iorB))^2
	
	1/(4pi) can be taken out as a constant
	(r + (1 - r) * (1 - LdotH)^5) * (R^2 / (NdotH^2 * (R^2 - 1) + 1)^2) * (1.0 / (1.0 + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotL^2)/NdotL^2) - 1) + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotV^2)/NdotV^2) - 1))) / (NdotV * NdotL)
	(r(1 - (1 - LdotH)^5) + (1 - LdotH)^5)

	Take out r as a constant as well. This splits the integral in two, where the full integral is calculated as Integral1 + r * Integral2.
	(r(1 - (1 - LdotH)^5) + (1 - LdotH)^5) * (R^2 / (NdotH^2 * (R^2 - 1) + 1)^2) * (1.0 / (1.0 + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotL^2)/NdotL^2) - 1) + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotV^2)/NdotV^2) - 1))) / (NdotV * NdotL)
	Integral1: (1 - LdotH)^5 * (R^2 / (NdotH^2 * (R^2 - 1) + 1)^2) * (1.0 / (1.0 + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotL^2)/NdotL^2) - 1) + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotV^2)/NdotV^2) - 1))) / (NdotV * NdotL)
	Integral2: (1 - (1 - LdotH)^5) * (R^2 / (NdotH^2 * (R^2 - 1) + 1)^2) * (1.0 / (1.0 + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotL^2)/NdotL^2) - 1) + 0.5 * (sqrt(1.0 + R^2 * (1.0 - NdotV^2)/NdotV^2) - 1))) / (NdotV * NdotL)
	
	*/

	F32 nDotV{ (F32(outputCoord.x) + 0.5) / F32(outputDim.x) };
	V3F toEye{ sqrt(1.0 - nDotV * nDotV), nDotV, 0.0 };
	F32 roughness{ (F32(outputCoord.y) + 0.5) / F32(outputDim.y) };
	roughness = roughness * roughness;

	F32 int1{ 0.0 };
	F32 int2{ 0.0 };
	U32 sampleCount{ 8192u };
	for U32 n{ 0 }; n < sampleCount; n = n + 1u {
		V3F half{ importance_sample_trowbridge_reitz(n, roughness) };
		V3F toLight{ normalize(reflect(-toEye, half)) };
		F32 nDotL{ toLight.y };
		if nDotL > 0.0 {
			F32 nDotH{ half.y };
			F32 vDotH{ dot(toEye, half) };
			F32 fresnel{ 1.0 - vDotH };
			fresnel = (fresnel * fresnel) * (fresnel * fresnel) * fresnel;
			/*
			Divide by nDotH because the PDF contained a factor of nDotH so it could integrate to 1, and we divided out the PDF (the rest of the PDF canceled with the NDF)
			Divide by nDotV because it's part of the BRDF. The divide by nDotL isn't present because it cancels with the multiply by nDotL when integrating the light
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
			F32 brdfPart{ (masking_shadowing_trowbridge_reitz(nDotL, nDotV, roughness) * vDotH) / (nDotH * nDotV) };
			int1 = int1 + fresnel * brdfPart;
			int2 = int2 + (1.0 - fresnel) * brdfPart;
		};
	};
	write_image(^outputTR, V3U(outputCoord, faceIdx), V4F(V2F(int1, int2) / F32(sampleCount), 0.0, 1.0));
};