#version 2
#shader compute

[set 0, binding 8, uniform] &Image2DStorageRG8 outputEON;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	V2U inputDimensions;
	V2U outputDimensions;
	// This is only used for the cubemap convolution part
	F32 roughness;
} pushConstants;

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

// "EON: A practical energy-preserving rough diffuse BRDF"
// https://arxiv.org/pdf/2410.18026
@[F32 g][F32 cosAngle] directional_albedo_g_approx_oren_nayar{
	F32 cosAngleInv{ 1.0 - cosAngle };
	// A polynomial approximation on 0 to pi/2 for (sin(x) * (x - sin(x) * cos(x)) + (2/3)(tan(x)(1-sin(x)^3) - sin(x))) / pi
	return cosAngleInv * (0.0571085289 + cosAngleInv * (0.491881867 + cosAngleInv * (-0.332181442 + 0.0714429953 * cosAngleInv)));
};
@[V3F eon][V3F diffuseColor, F32 vDotL, F32 nDotL, F32 nDotV, F32 roughness] diffuse_energy_preserving_oren_nayar{
	F32 PI{ 3.14159265 };
	F32 PI_RCP{ 0.31830989 };
	F32 s{ vDotL - nDotL * nDotV };
	F32 tRcp{ if s <= 0.0 { 1.0 } else { 1.0 / max(nDotL, nDotV) } };
	F32 A{ 1.0 / (1.0 + (0.5 - 2.0 / (3.0 * PI)) * roughness) };
	F32 B{ roughness * A };

	F32 albedoAverage{ A + (2.0 / 3.0 - 28.0 / (15.0 * PI)) * B };
	V3F colorMultiscattering{ diffuseColor * diffuseColor * PI_RCP * (albedoAverage / (1.0 - diffuseColor * (1.0 - albedoAverage))) };
	F32 directionalAlbedo1{ A + B * directional_albedo_g_approx_oren_nayar(nDotL) };
	F32 directionalAlbedo2{ A + B * directional_albedo_g_approx_oren_nayar(nDotV) };

	V3F diffuseOrenNayar{ diffuseColor * PI_RCP * (A + B * s * tRcp);
	V3F energyCompensation{ colorMultiscattering * ((1.0 - directionalAlbedo1) * (1.0 - directionalAlbedo2) / (1.0 - albedoAverage));
	return diffuseOrenNayar + energyCompensation;
};

[entrypoint, localsize 16 16 1] @[][] compute_main{
	V2U outputCoord{ globalInvocationId.xy };
	V2U outputDim{ pushConstants.outputDimensions };
	if outputCoord.x >= outputDim.x || outputCoord.y >= outputDim.y {
		return;
	};

	// "EON: A practical energy-preserving rough diffuse BRDF"
	// https://arxiv.org/pdf/2410.18026
	// This is the BRDF part of the split sum approximation (assuming integral[vary normal and view and roughness](L * BRDF) ~= integral[vary normal and roughness, assume E=N](L * BRDF) * integral[vary nDotV and roughness, N=constant](BRDF))
	/*
	For energy preserving oren nayar, we're trying to integrate this function
	albedoAverage = (A + (2.0 / 3.0 - 28.0 / (15.0 * PI)) * B))
	colorMultiscattering = (diffuseColor * diffuseColor * albedoAverage / (1 - diffuseColor * (1 - albedoAverage)) / PI)
	comp = colorMultiscattering * (1 - (A + B * polyGApprox(nDotL, roughness))) * (1 - (A + B * polyGApprox(NdotV, roughness))) / (1 - albedoAverage)
	if VdotL - NdotL * NdotV > 0: (A + B * (VdotL - NdotL * NdotV) / max(NdotL, NdotV)) / PI + comp
	else: (A + B * (VdotL - NdotL * NdotV)) / PI + comp

	diffuseColor is specific to the fragment being shaded, so we can't have it in the integral
	diffuseColor is taken out of the primary term as a constant multiple
	colorMultiscattering is only dependent on diffuseColor and roughness, so that can be taken out as a constant multiple as well

	So, we get two function to integrate
	r = VdotL - NdotL * NdotV > 0 ? (A + B * (VdotL - NdotL * NdotV) / max(NdotL, NdotV)) / PI : (A + B * (VdotL - NdotL * NdotV)) / PI
	g = (1 - (A + B * polyGApprox(nDotL, roughness))) * (1 - (A + B * polyGApprox(NdotV, roughness))) / (1 - albedoAverage)
	*/

	F32 nDotV{ (F32(outputCoord.x) + 0.5) / F32(outputDim.x) };
	V3F toEye{ sqrt(1.0 - nDotV * nDotV), nDotV, 0.0 };
	F32 roughness{ (F32(outputCoord.y) + 0.5) / F32(outputDim.y) };
	roughness = roughness * roughness;

	F32 PI{ 3.14159265 };
	F32 PI_RCP{ 0.31830989 };
	F32 A{ 1.0 / (1.0 + (0.5 - 2.0 / (3.0 * PI)) * roughness) };
	F32 B{ roughness * A };
	F32 albedoAverage{ A + (2.0 / 3.0 - 28.0 / (15.0 * PI)) * B };

	V2F integral{ 0.0, 0.0 };
	U32 samplesPhi{ 1024u };
	U32 samplesTheta{ 256u };
	for U32 i{ 0u }; i < samplesPhi; i = i + 1 {
		for U32 j{ 0u }; j < samplesTheta; j = j + 1 {
			F32 t{ F32(j) / 256.0 * 0.5 * PI };
			F32 p{ F32(i) / 1024.0 * 2.0 * PI };
			V3F toLight{ cos(p) * cos(t), sin(t), sin(p) * cos(t) };
			F32 nDotL{ toLight.y };
			F32 vDotL{ dot(toEye, toLight) };
			F32 s{ vDotL - nDotL * nDotV };
			F32 tRcp{ if s <= 0.0 { 1.0 } else { 1.0 / max(nDotL, nDotV) } };
			F32 diffuse{ (A + B * PI_RCP * s * tRcp };
			F32 comp{ (1 - (A + B * directional_albedo_g_approx_oren_nayar(nDotL))) * (1 - (A + B * directional_albedo_g_approx_oren_nayar(NdotV))) / (1 - albedoAverage) };
			integral = integral + V2F(diffuse, comp) * sin(t) * nDotL;
		};
	};
	write_image(^outputEON, outputCoord, V4F(integral / (F32(samplesPhi) * F32(samplesTheta)), 0.0, 1.0));
};