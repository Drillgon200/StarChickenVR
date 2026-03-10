#pragma once

#include "../DrillLib.h"
#include "BCCommon.h"

#pragma pack(push, 1)

namespace BC7 {
// This code was written years before this project got started, so its style is a bit more C++ than the rest of the project

#include "BC7Tables.h"

struct BC7Block {
	U64 data[2];
};

struct alignas(32) BC7Blockx4 {
	U64x4 data[2];
};

#pragma pack(pop)

// Useful resources
// https://registry.khronos.org/DataFormat/specs/1.3/dataformat.1.3.html#bptc_bc7
// https://docs.microsoft.com/en-us/windows/win32/direct3d11/bc7-format
// https://docs.microsoft.com/en-us/windows/win32/direct3d11/bc7-format-mode-reference

FINLINE U32x8 bc7_interpolate(const U32x8& e0, const U32x8& e1, const U32x8& interpolationFactor) {
	const U32x8 leftInterp = _mm256_mullo_epi32(_mm256_sub_epi32(_mm256_set1_epi32(64), interpolationFactor), e0);
	const U32x8 rightInterp = _mm256_mullo_epi32(interpolationFactor, e1);
	return _mm256_srli_epi32(_mm256_add_epi32(_mm256_add_epi32(leftInterp, rightInterp), _mm256_set1_epi32(32)), 6);
}

template<typename T>
T bc7_interpolatex8(const T& a, const T& b, const U32x8& interpolationFactor, const U32x8& interpolationFactorAlpha) {
	static_assert(typeid(T) == typeid(F32x8) || typeid(T) == typeid(V3Fx8) || typeid(T) == typeid(V4Fx8), "Implement type");
}

template<>
FINLINE F32x8 bc7_interpolatex8(const F32x8& a, const F32x8& b, const U32x8& interpolationFactor, const U32x8& interpolationFactorAlpha) {
	return _mm256_cvtepi32_ps(bc7_interpolate(_mm256_cvtps_epi32(a), _mm256_cvtps_epi32(b), interpolationFactor));
}

template<>
FINLINE V3Fx8 bc7_interpolatex8(const V3Fx8& a, const V3Fx8& b, const U32x8& interpolationFactor, const U32x8& interpolationFactorAlpha) {
	return V3Fx8{
		_mm256_cvtepi32_ps(bc7_interpolate(_mm256_cvtps_epi32(a.x), _mm256_cvtps_epi32(b.x), interpolationFactor)),
		_mm256_cvtepi32_ps(bc7_interpolate(_mm256_cvtps_epi32(a.y), _mm256_cvtps_epi32(b.y), interpolationFactor)),
		_mm256_cvtepi32_ps(bc7_interpolate(_mm256_cvtps_epi32(a.z), _mm256_cvtps_epi32(b.z), interpolationFactor))
	};
}

template<>
FINLINE V4Fx8 bc7_interpolatex8(const V4Fx8& a, const V4Fx8& b, const U32x8& interpolationFactor, const U32x8& interpolationFactorAlpha) {
	return V4Fx8{
		_mm256_cvtepi32_ps(bc7_interpolate(_mm256_cvtps_epi32(a.x), _mm256_cvtps_epi32(b.x), interpolationFactor)),
		_mm256_cvtepi32_ps(bc7_interpolate(_mm256_cvtps_epi32(a.y), _mm256_cvtps_epi32(b.y), interpolationFactor)),
		_mm256_cvtepi32_ps(bc7_interpolate(_mm256_cvtps_epi32(a.z), _mm256_cvtps_epi32(b.z), interpolationFactor)),
		_mm256_cvtepi32_ps(bc7_interpolate(_mm256_cvtps_epi32(a.w), _mm256_cvtps_epi32(b.w), interpolationFactorAlpha))
	};
}

template<typename Endpoint>
FINLINE F32x8 get_distance_between_endpoints(const Endpoint endpoints[2], const Endpoint& pixel) {
	Endpoint endpointVector = endpoints[1] - endpoints[0];
	Endpoint pixelVector = pixel - endpoints[0];
	F32x8 project = dot(endpointVector, pixelVector);
	return clamp01(_mm256_div_ps(project, length_sq(endpointVector)));
}

template<U32 bits>
FINLINE F32x8 quantize_to_index(const V4Fx8 endpoints[2], const V4Fx8& pixel) {
	constexpr F32 scale = static_cast<F32>((1 << bits) - 1);
	F32x8 normalizedProject = get_distance_between_endpoints(endpoints, pixel);
	// trunc((normalizedProject * scale + 0.5)) / scale
	return _mm256_mul_ps(_mm256_round_ps(_mm256_add_ps(_mm256_mul_ps(normalizedProject, _mm256_set1_ps(scale)), _mm256_set1_ps(0.5F)), _MM_FROUND_TRUNC), _mm256_set1_ps(1.0F / scale));
}

template<U32 bits>
FINLINE F32x8 quantize_to_index(const V3Fx8 endpoints[2], const V3Fx8& pixel) {
	constexpr F32 scale = static_cast<F32>((1 << bits) - 1);
	F32x8 normalizedProject = get_distance_between_endpoints(endpoints, pixel);
	// trunc((normalizedProject * scale + 0.5)) / scale
	return _mm256_mul_ps(_mm256_round_ps(_mm256_add_ps(_mm256_mul_ps(normalizedProject, _mm256_set1_ps(scale)), _mm256_set1_ps(0.5F)), _MM_FROUND_TRUNC), _mm256_set1_ps(1.0F / scale));
}

template<U32 bits>
FINLINE F32x8 quantize_to_index(const F32x8 endpoints[2], const F32x8& pixel) {
	constexpr F32 scale = static_cast<F32>((1 << bits) - 1);
	F32x8 normalizedProject = _mm256_div_ps(_mm256_sub_ps(pixel, endpoints[0]), _mm256_sub_ps(endpoints[1], endpoints[0]));
	// trunc((normalizedProject * scale + 0.5)) / scale
	return _mm256_mul_ps(_mm256_round_ps(_mm256_add_ps(_mm256_mul_ps(normalizedProject, _mm256_set1_ps(scale)), _mm256_set1_ps(0.5F)), _MM_FROUND_TRUNC), _mm256_set1_ps(1.0F / scale));
}

template<U32 bits>
FINLINE U32x8 find_index(const V4Fx8 endpoints[2], const V4Fx8& pixel) {
	constexpr F32 scale = static_cast<F32>((1 << bits) - 1);
	F32x8 normalizedProject = get_distance_between_endpoints(endpoints, pixel);
	// (U32) trunc(normalizedProject * scale * 0.5)
	return _mm256_cvtps_epi32(_mm256_round_ps(_mm256_add_ps(_mm256_mul_ps(normalizedProject, _mm256_set1_ps(scale)), _mm256_set1_ps(0.5F)), _MM_FROUND_TRUNC));
}

template<U32 bits>
FINLINE U32x8 find_index(const V3Fx8 endpoints[2], const V3Fx8& pixel) {
	constexpr F32 scale = static_cast<F32>((1 << bits) - 1);
	F32x8 normalizedProject = get_distance_between_endpoints(endpoints, pixel);
	// (U32) trunc(normalizedProject * scale * 0.5)
	return _mm256_cvtps_epi32(_mm256_round_ps(_mm256_add_ps(_mm256_mul_ps(normalizedProject, _mm256_set1_ps(scale)), _mm256_set1_ps(0.5F)), _MM_FROUND_TRUNC));
}

template<U32 bits>
FINLINE U32x8 find_index(const F32x8 endpoints[2], const F32x8& pixel) {
	constexpr F32 scale = static_cast<F32>((1 << bits) - 1);
	F32x8 normalizedProject = _mm256_div_ps(_mm256_sub_ps(pixel, endpoints[0]), _mm256_sub_ps(endpoints[1], endpoints[0]));
	// (U32) trunc(normalizedProject * scale * 0.5)
	return _mm256_cvtps_epi32(_mm256_round_ps(_mm256_add_ps(_mm256_mul_ps(normalizedProject, _mm256_set1_ps(scale)), _mm256_set1_ps(0.5F)), _MM_FROUND_TRUNC));
}


U8 bc7_interpolate(U8 e0, U8 e1, U32 interpolationFactor) {
	return ((64 - interpolationFactor) * e0 + interpolationFactor * e1 + 32) >> 6;
}

template<typename T>
T bc7_interpolate(T a, T b, U32 interpolationFactor, U32 interpolationFactorAlpha) {
	static_assert(typeid(T) == typeid(F32) || typeid(T) == typeid(V3F) || typeid(T) == typeid(V4F), "Implement type");
}

template<>
F32 bc7_interpolate(F32 a, F32 b, U32 interpolationFactor, U32 interpolationFactorAlpha) {
	return F32(bc7_interpolate(U8(a), U8(b), interpolationFactor));
}

template<>
V3F bc7_interpolate(V3F a, V3F b, U32 interpolationFactor, U32 interpolationFactorAlpha) {
	return V3F{
		bc7_interpolate(a.x, b.x, interpolationFactor, 0),
		bc7_interpolate(a.y, b.y, interpolationFactor, 0),
		bc7_interpolate(a.z, b.z, interpolationFactor, 0)
	};
}

template<>
V4F bc7_interpolate(V4F a, V4F b, U32 interpolationFactor, U32 interpolationFactorAlpha) {
	return V4F{
		bc7_interpolate(a.x, b.x, interpolationFactor, 0),
		bc7_interpolate(a.y, b.y, interpolationFactor, 0),
		bc7_interpolate(a.z, b.z, interpolationFactor, 0),
		bc7_interpolate(a.w, b.w, interpolationFactorAlpha, 0)
	};
}

template<typename Vec>
F32 get_distance_between_endpoints(Vec endpoints[2], Vec pixel) {
	Vec endpointVector = endpoints[1] - endpoints[0];
	Vec pixelVector = pixel - endpoints[0];
	F32 project = dot(endpointVector, pixelVector);
	return clamp01(project / length_sq(endpointVector));
}

template<U32 bits>
F32 quantize_to_index(V4F endpoints[2], V4F pixel) {
	const F32 scale = F32((1 << bits) - 1);
	const F32 invScale = 1.0F / scale;
	F32 normalizedProject = get_distance_between_endpoints(endpoints, pixel);
	return truncf32(normalizedProject * scale + 0.5) * invScale;
}

template<U32 bits>
F32 quantize_to_index(V3F endpoints[2], V3F pixel) {
	const F32 scale = F32((1 << bits) - 1);
	const F32 invScale = 1.0F / scale;
	F32 normalizedProject = get_distance_between_endpoints(endpoints, pixel);
	return truncf32(normalizedProject * scale + 0.5) * invScale;
}

template<U32 bits>
F32 quantize_to_index(F32 endpoints[2], F32 pixel) {
	const F32 scale = F32((1 << bits) - 1);
	const F32 invScale = 1.0F / scale;
	F32 normalizedProject = clamp01((pixel - endpoints[0]) / (endpoints[1] - endpoints[0]));
	return truncf32(normalizedProject * scale + 0.5) * invScale;
}

template<U32 bits>
U32 find_index(V4F endpoints[2], V4F pixel) {
	const F32 scale = F32((1 << bits) - 1);
	F32 normalizedProject = get_distance_between_endpoints(endpoints, pixel);
	return U32(truncf32(normalizedProject * scale + 0.5F));
}

template<U32 bits>
U32 find_index(V3F endpoints[2], V3F pixel) {
	const F32 scale = F32((1 << bits) - 1);
	F32 normalizedProject = get_distance_between_endpoints(endpoints, pixel);
	return U32(truncf32(normalizedProject * scale + 0.5F));
}

template<U32 bits>
U32 find_index(F32 endpoints[2], F32 pixel) {
	const F32 scale = F32((1 << bits) - 1);
	F32 normalizedProject = clamp01((pixel - endpoints[0]) / (endpoints[1] - endpoints[0]));
	return U32(truncf32(normalizedProject * scale + 0.5F));
}

F32 ray_cast_unit_box(F32 pos, F32 dir) {
	F32 invDir = 1.0F / dir;
	F32 tMin = -pos * invDir;
	F32 tMax = (1.0F - pos) * invDir;
	return max(tMin, tMax);
}

F32 ray_cast_unit_box(V3F pos, V3F dir) {
	V3F invDir = 1.0F / dir;

	F32 xTMin = (-pos.x) * invDir.x;
	F32 xTMax = (1.0F - pos.x) * invDir.x;
	F32 yTMin = (-pos.y) * invDir.y;
	F32 yTMax = (1.0F - pos.y) * invDir.y;
	F32 zTMin = (-pos.z) * invDir.z;
	F32 zTMax = (1.0F - pos.z) * invDir.z;

	return min(max(xTMax, xTMin), min(max(yTMax, yTMin), max(zTMax, zTMin)));
}

F32 ray_cast_unit_box(V4F pos, V4F dir) {
	V4F invDir = 1.0F / dir;

	F32 xTMin = (-pos.x) * invDir.x;
	F32 xTMax = (1.0F - pos.x) * invDir.x;
	F32 yTMin = (-pos.y) * invDir.y;
	F32 yTMax = (1.0F - pos.y) * invDir.y;
	F32 zTMin = (-pos.z) * invDir.z;
	F32 zTMax = (1.0F - pos.z) * invDir.z;
	F32 wTMin = (-pos.w) * invDir.w;
	F32 wTMax = (1.0F - pos.w) * invDir.w;

	return min(max(xTMax, xTMin), min(max(yTMax, yTMin), min(max(zTMax, zTMin), max(wTMax, wTMin))));
}

F32x8 ray_cast_unit_box(const F32x8& pos, const F32x8& dir) {
	F32x8 invDir = _mm256_rcp_ps(dir);
	F32x8 tMin = _mm256_mul_ps(_mm256_sub_ps(_mm256_setzero_ps(), pos), invDir);
	F32x8 tMax = _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(1.0F), pos), invDir);
	return _mm256_max_ps(tMin, tMax);
}

F32x8 ray_cast_unit_box(const V3Fx8& pos, const V3Fx8& dir) {
	V3Fx8 invDir = rcp(dir);

	F32x8 xTMin = _mm256_mul_ps(_mm256_sub_ps(_mm256_setzero_ps(), pos.x), invDir.x);
	F32x8 xTMax = _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(1.0F), pos.x), invDir.x);
	F32x8 yTMin = _mm256_mul_ps(_mm256_sub_ps(_mm256_setzero_ps(), pos.y), invDir.y);
	F32x8 yTMax = _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(1.0F), pos.y), invDir.y);
	F32x8 zTMin = _mm256_mul_ps(_mm256_sub_ps(_mm256_setzero_ps(), pos.z), invDir.z);
	F32x8 zTMax = _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(1.0F), pos.z), invDir.z);

	return _mm256_min_ps(_mm256_max_ps(xTMax, xTMin), _mm256_min_ps(_mm256_max_ps(yTMax, yTMin), _mm256_max_ps(zTMax, zTMin)));
}

F32x8 ray_cast_unit_box(const V4Fx8& pos, const V4Fx8& dir) {
	V4Fx8 invDir = rcp(dir);

	F32x8 xTMin = _mm256_mul_ps(_mm256_sub_ps(_mm256_setzero_ps(), pos.x), invDir.x);
	F32x8 xTMax = _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(1.0F), pos.x), invDir.x);
	F32x8 yTMin = _mm256_mul_ps(_mm256_sub_ps(_mm256_setzero_ps(), pos.y), invDir.y);
	F32x8 yTMax = _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(1.0F), pos.y), invDir.y);
	F32x8 zTMin = _mm256_mul_ps(_mm256_sub_ps(_mm256_setzero_ps(), pos.z), invDir.z);
	F32x8 zTMax = _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(1.0F), pos.z), invDir.z);
	F32x8 wTMin = _mm256_mul_ps(_mm256_sub_ps(_mm256_setzero_ps(), pos.w), invDir.w);
	F32x8 wTMax = _mm256_mul_ps(_mm256_sub_ps(_mm256_set1_ps(1.0F), pos.w), invDir.w);

	return _mm256_min_ps(_mm256_max_ps(xTMax, xTMin), _mm256_min_ps(_mm256_max_ps(yTMax, yTMin), _mm256_min_ps(_mm256_max_ps(zTMax, zTMin), _mm256_max_ps(wTMax, wTMin))));
}

template<typename Endpoint>
FINLINE Endpoint convert_v4fx8_to_endpoint(V4Fx8 v) {
	static_assert(false, "Unimplemented endpoint");
}
template<>
FINLINE F32x8 convert_v4fx8_to_endpoint<F32x8>(V4Fx8 v) {
	return v.w;
}
template<>
FINLINE V3Fx8 convert_v4fx8_to_endpoint<V3Fx8>(V4Fx8 v) {
	return V3Fx8{ v.x, v.y, v.z };
}
template<>
FINLINE V4Fx8 convert_v4fx8_to_endpoint<V4Fx8>(V4Fx8 v) {
	return v;
}
template<typename Endpoint>
FINLINE Endpoint convert_v4f_to_endpoint(V4F v) {
	static_assert(false, "Unimplemented endpoint");
}
template<>
FINLINE F32 convert_v4f_to_endpoint<F32>(V4F v) {
	return v.w;
}
template<>
FINLINE V3F convert_v4f_to_endpoint<V3F>(V4F v) {
	return V3F{ v.x, v.y, v.z };
}
template<>
FINLINE V4F convert_v4f_to_endpoint<V4F>(V4F v) {
	return v;
}

// No operator overloads on default types so I need this (I should just make a struct wrapper for F32x8 instead of a typedef)
template<typename A, typename B>
FINLINE auto add(A a, B b) {
	return a + b;
}
template<>
FINLINE auto add<F32x8, F32x8>(F32x8 a, F32x8 b) {
	return _mm256_add_ps(a, b);
}
template<typename A, typename B>
FINLINE auto sub(A a, B b) {
	return a - b;
}
template<>
FINLINE auto sub<F32x8, F32x8>(F32x8 a, F32x8 b) {
	return _mm256_sub_ps(a, b);
}
template<typename A, typename B>
FINLINE auto mul(A a, B b) {
	return a * b;
}
template<>
FINLINE auto mul<F32x8, F32x8>(F32x8 a, F32x8 b) {
	return _mm256_mul_ps(a, b);
}


// This one does one partition at a time
template<typename Endpoint, U32 indexResolution>
void least_squares_optimize_endpoints(V4Fx8 pixels[16], Endpoint endpoints[2], const BC7PartitionTable& table, U32 partition) {
	F32x8 endpointsEqual = endpoints[0] == endpoints[1];
	if (_mm256_movemask_ps(endpointsEqual) == 0xFF) {
		// All of them have no axis to optimize along
		return;
	}
	F32x8 alphaSq{};
	F32x8 alphaBeta{};
	F32x8 betaSq{};
	Endpoint alphaX{};
	Endpoint betaX{};
	for (U32 i = 0; i < 16; i++) {
		if (table.partitionNumbers[i] != partition) {
			continue;
		}
		Endpoint pixel = convert_v4fx8_to_endpoint<Endpoint>(pixels[i]);

		F32x8 alpha = quantize_to_index<indexResolution>(endpoints, pixel);
		F32x8 beta = _mm256_sub_ps(_mm256_set1_ps(1.0F), alpha);

		alphaSq = _mm256_fmadd_ps(alpha, alpha, alphaSq);
		alphaBeta = _mm256_fmadd_ps(alpha, beta, alphaBeta);
		betaSq = _mm256_fmadd_ps(beta, beta, betaSq);

		alphaX = fmadd(alpha, pixel, alphaX);
		betaX = fmadd(beta, pixel, betaX);
	}

	F32x8 inverseDeterminant = _mm256_rcp_ps(_mm256_fmsub_ps(alphaSq, betaSq, _mm256_mul_ps(alphaBeta, alphaBeta)));
	alphaBeta = _mm256_sub_ps(_mm256_setzero_ps(), _mm256_mul_ps(alphaBeta, inverseDeterminant));
	alphaSq = _mm256_mul_ps(alphaSq, inverseDeterminant);
	betaSq = _mm256_mul_ps(betaSq, inverseDeterminant);

	endpoints[0] = blend(fmadd(betaSq, alphaX, mul(alphaBeta, betaX)), endpoints[0], endpointsEqual);
	endpoints[1] = blend(fmadd(alphaBeta, alphaX, mul(alphaSq, betaX)), endpoints[1], endpointsEqual);
}

// This one does all partitions at once
template<typename Endpoint, U32 indexResolution, U32 partitions>
void least_squares_optimize_endpointsx8(V4Fx8 pixels[16], Endpoint endpoints[6], const BC7PartitionTable& table) {
	F32x8 endpointsEqual[3]{ endpoints[0] == endpoints[1], endpoints[2] == endpoints[3], endpoints[4] == endpoints[5] };
	U32 endpointsEqualMasks[3]{ _mm256_movemask_ps(endpointsEqual[0]), _mm256_movemask_ps(endpointsEqual[1]), _mm256_movemask_ps(endpointsEqual[2]) };
	if ((endpointsEqualMasks[0] & endpointsEqualMasks[1] & endpointsEqualMasks[2]) == 0xFF) {
		// All of them have no axis to optimize along
		return;
	}
	F32x8 alphaSq[partitions]{};
	F32x8 alphaBeta[partitions]{};
	F32x8 betaSq[partitions]{};
	Endpoint alphaX[partitions]{};
	Endpoint betaX[partitions]{};
	for (U32 i = 0; i < 16; i++) {
		U32 partition = table.partitionNumbers[i];
		if (endpointsEqualMasks[partition] == 0xFF) {
			continue;
		}
		Endpoint pixel = convert_v4fx8_to_endpoint<Endpoint>(pixels[i]);

		F32x8 alpha = quantize_to_index<indexResolution>(&endpoints[partition * 2], pixel);
		F32x8 beta = _mm256_sub_ps(_mm256_set1_ps(1.0F), alpha);

		alphaSq[partition] = _mm256_fmadd_ps(alpha, alpha, alphaSq[partition]);
		alphaBeta[partition] = _mm256_fmadd_ps(alpha, beta, alphaBeta[partition]);
		betaSq[partition] = _mm256_fmadd_ps(beta, beta, betaSq[partition]);

		alphaX[partition] = fmadd(alpha, pixel, alphaX[partition]);
		betaX[partition] = fmadd(beta, pixel, betaX[partition]);
	}

	for (U32 i = 0; i < partitions; i++) {
		if (endpointsEqualMasks[i] == 0xFF) {
			continue;
		}
		F32x8 inverseDeterminant = _mm256_rcp_ps(_mm256_fmsub_ps(alphaSq[i], betaSq[i], _mm256_mul_ps(alphaBeta[i], alphaBeta[i])));
		F32x8 alphaBetaInv = _mm256_sub_ps(_mm256_setzero_ps(), _mm256_mul_ps(alphaBeta[i], inverseDeterminant));
		F32x8 alphaSqInv = _mm256_mul_ps(alphaSq[i], inverseDeterminant);
		F32x8 betaSqInv = _mm256_mul_ps(betaSq[i], inverseDeterminant);

		endpoints[i * 2 + 0] = blend(fmadd(betaSqInv, alphaX[i], mul(alphaBetaInv, betaX[i])), endpoints[i * 2 + 0], endpointsEqual[i]);
		endpoints[i * 2 + 1] = blend(fmadd(alphaBetaInv, alphaX[i], mul(alphaSqInv, betaX[i])), endpoints[i * 2 + 1], endpointsEqual[i]);
	}
}

template<U32 indexBits>
void least_squares_optimize_endpoints_RGBA8(V4F pixels[16], V4F endpoints[2], const BC7PartitionTable& table, U32 partition) {
	if (endpoints[0] == endpoints[1]) {
		// No starting axis to optimize along, the endpoint will be the exact color
		return;
	}
	F32 alphaSq = 0.0F;
	F32 alphaBeta = 0.0F;
	F32 betaSq = 0.0F;
	V4F alphaX{ 0.0F };
	V4F betaX{ 0.0F };
	// Find least squares best fit for indices
	for (U32 i = 0; i < 16; i++) {
		if (table.partitionNumbers[i] != partition) {
			continue;
		}
		F32 alpha = quantize_to_index<indexBits>(endpoints, pixels[i]);
		F32 beta = 1.0F - alpha;

		alphaSq += alpha * alpha;
		alphaBeta += alpha * beta;
		betaSq += beta * beta;

		alphaX += alpha * pixels[i];
		betaX += beta * pixels[i];
	}

	// Inverse matrix
	F32 inverseDeterminant = 1.0F / (alphaSq * betaSq - alphaBeta * alphaBeta);
	alphaBeta = -alphaBeta * inverseDeterminant;
	alphaSq *= inverseDeterminant;
	betaSq *= inverseDeterminant;

	endpoints[0] = betaSq * alphaX + alphaBeta * betaX;
	endpoints[1] = alphaBeta * alphaX + alphaSq * betaX;
}

template<U32 indexBits>
void least_squares_optimize_endpoints_rgb(V4F pixels[16], V3F endpoints[2], const BC7PartitionTable& table, U32 partition) {
	if (endpoints[0] == endpoints[1]) {
		// No starting axis to optimize along, the endpoint will be the exact color
		return;
	}
	F32 alphaSq = 0.0F;
	F32 alphaBeta = 0.0F;
	F32 betaSq = 0.0F;
	V3F alphaX{ 0.0F, 0.0F, 0.0F };
	V3F betaX{ 0.0F, 0.0F, 0.0F };
	// Find least squares best fit for indices
	for (U32 i = 0; i < 16; i++) {
		if (table.partitionNumbers[i] != partition) {
			continue;
		}
		V4F pixel4 = pixels[i];
		V3F pixel3{ pixel4.x, pixel4.y, pixel4.z };
		F32 alpha = quantize_to_index<indexBits>(endpoints, pixel3);
		F32 beta = 1.0F - alpha;

		alphaSq += alpha * alpha;
		alphaBeta += alpha * beta;
		betaSq += beta * beta;
		
		alphaX += alpha * pixel3;
		betaX += beta * pixel3;
	}

	// Inverse matrix
	F32 inverseDeterminant = 1.0F / (alphaSq * betaSq - alphaBeta * alphaBeta);
	alphaBeta = -alphaBeta * inverseDeterminant;
	alphaSq *= inverseDeterminant;
	betaSq *= inverseDeterminant;

	endpoints[0] = betaSq * alphaX + alphaBeta * betaX;
	endpoints[1] = alphaBeta * alphaX + alphaSq * betaX;
}

template<U32 indexBits>
void least_squares_optmize_endpoints_alpha(V4F pixels[16], F32 endpoints[2]) {
	if (endpoints[0] == endpoints[1]) {
		// No starting axis to optimize along, the endpoint will be the exact color
		return;
	}
	F32 alphaSq = 0.0F;
	F32 alphaBeta = 0.0F;
	F32 betaSq = 0.0F;
	F32 alphaX{ 0.0F };
	F32 betaX{ 0.0F };
	// Find least squares best fit for indices
	for (U32 i = 0; i < 16; i++) {
		F32 alpha = quantize_to_index<indexBits>(endpoints, pixels[i].w);
		F32 beta = 1.0F - alpha;

		alphaSq += alpha * alpha;
		alphaBeta += alpha * beta;
		betaSq += beta * beta;

		alphaX += alpha * pixels[i].w;
		betaX += beta * pixels[i].w;
	}

	// Inverse matrix
	F32 inverseDeterminant = 1.0F / (alphaSq * betaSq - alphaBeta * alphaBeta);
	alphaBeta = -alphaBeta * inverseDeterminant;
	alphaSq *= inverseDeterminant;
	betaSq *= inverseDeterminant;

	endpoints[0] = betaSq * alphaX + alphaBeta * betaX;
	endpoints[1] = alphaBeta * alphaX + alphaSq * betaX;
}

const U32 numPartitionTablesPerSubset = 64;
const U32 numPartitionsFor3Subsets = numPartitionTablesPerSubset * 3;
const U32 numPartitionsFor2Subsets = numPartitionTablesPerSubset * 2;
const U32 totalNumPartitions = numPartitionsFor3Subsets + numPartitionsFor2Subsets;

F32 distance_to_line_sq(V3F point, V3F line[2]) {
	V3F lineVector = line[1] - line[0];
	F32 proj = dot(point - line[0], lineVector) / length_sq(lineVector);
	return length_sq(point - (line[0] + proj * lineVector));
}

F32 pixels_dist_to_line_sq(V4F pixels[16], V4F line[2]) {
	F32 dist = 0;
	V4F lineVector = line[1] - line[0];
	for (U32 i = 0; i < 16; i++) {
		V4F point = pixels[i];
		F32 proj = dot(point - line[0], lineVector) / length_sq(lineVector);
		return length_sq(point - (line[0] + proj * lineVector));
	}
	return dist;
}

template<U32 numPartitions>
void choose_best_diagonals_RGBA8(V4F pixels[16], V4F boundingBoxes[numPartitions * 2], const BC7PartitionTable& table) {
	for (U32 part = 0; part < numPartitions; part++) {
		V4F& min = boundingBoxes[part * 2 + 0];
		V4F& max = boundingBoxes[part * 2 + 1];

		V4F bestDiag[2]{ min, max };
		F32 bestError = pixels_dist_to_line_sq(pixels, bestDiag);

		V4F diag[2]{ min, max };
		F32 error;

#define CHECK_DIAG error = pixels_dist_to_line_sq(pixels, diag);\
		if (error < bestError) {\
			bestError = error;\
			memcpy(bestDiag, diag, 2 * sizeof(V4F));\
		}

		swap(&diag[0].x, &diag[1].x);
		CHECK_DIAG;

		swap(&diag[0].y, &diag[1].y);
		CHECK_DIAG;

		swap(&diag[0].x, &diag[1].x);
		CHECK_DIAG;

		swap(&diag[0].z, &diag[1].z);
		CHECK_DIAG;

		swap(&diag[0].x, &diag[1].x);
		CHECK_DIAG;

		swap(&diag[0].y, &diag[1].y);
		CHECK_DIAG;

		swap(&diag[0].x, &diag[1].x);
		CHECK_DIAG;

#undef CHECK_DIAG

		memcpy(&boundingBoxes[part * 2], bestDiag, 2 * sizeof(V4F));
	}
}

template<U32 numPartitions>
void choose_best_diagonals(V4F pixels[16], V3F boundingBoxes[6], const BC7PartitionTable& table) {
	const U32 diagonalSelectors[3 * 3] = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
	//3 endpoint pairs, 3 diagonals for each one
	F32 distancesSq[numPartitions][3]{};
	for (U32 i = 0; i < 16; i++) {
		U32 partition = table.partitionNumbers[i];
		V3F* endpoints = &boundingBoxes[partition * 2];
		// I JUST LOOKED AT THIS AGAIN AND IT TURNS OUT THERE ARE 4 DIAGONALS ACCROSS A 3D BOX
		// I'm stupid
		for (U32 diag = 0; diag < 3; diag++) {
			V3F diagonal[]{
				V3F{ endpoints[diagonalSelectors[diag * 3 + 0]].x, endpoints[diagonalSelectors[diag * 3 + 1]].y, endpoints[diagonalSelectors[diag * 3 + 2]].z },
				V3F{ endpoints[1 - diagonalSelectors[diag * 3 + 0]].x, endpoints[1 - diagonalSelectors[diag * 3 + 1]].y, endpoints[1 - diagonalSelectors[diag * 3 + 2]].z }
			};
			distancesSq[partition][diag] += distance_to_line_sq(v3f_xyz(pixels[i]), diagonal);
		}
	}
	for (U32 partition = 0; partition < numPartitions; partition++) {
		if (boundingBoxes[partition * 2] == boundingBoxes[partition * 2 + 1]) {
			continue;
		}
		U32 minDistanceDiag;
		F32 minDistance = F32_LARGE;
		for (U32 diag = 0; diag < 3; diag++) {
			F32 dist = distancesSq[partition][diag];
			if (dist < minDistance) {
				minDistanceDiag = diag;
				minDistance = dist;
			}
		}
		V3F* endpoints = &boundingBoxes[partition * 2];
		V3F diagonal[]{
			V3F{ endpoints[diagonalSelectors[minDistanceDiag * 3 + 0]].x, endpoints[diagonalSelectors[minDistanceDiag * 3 + 1]].y, endpoints[diagonalSelectors[minDistanceDiag * 3 + 2]].z },
			V3F{ endpoints[1 - diagonalSelectors[minDistanceDiag * 3 + 0]].x, endpoints[1 - diagonalSelectors[minDistanceDiag * 3 + 1]].y, endpoints[1 - diagonalSelectors[minDistanceDiag * 3 + 2]].z }
		};
		boundingBoxes[partition * 2 + 0] = diagonal[0];
		boundingBoxes[partition * 2 + 1] = diagonal[1];
	}
}

template<typename Endpoint>
FINLINE F32x8 pixels_dist_to_line_sq(const V4Fx8 pixels[16], const Endpoint line[2], const U32 partition, const BC7PartitionTable& table) {
	F32x8 distSq = _mm256_setzero_ps();
	for (U32 i = 0; i < 16; i++) {
		if (table.partitionNumbers[i] != partition) {
			continue;
		}
		Endpoint lineVector = line[1] - line[0];
		Endpoint point = convert_v4fx8_to_endpoint<Endpoint>(pixels[i]);
		F32x8 proj = _mm256_div_ps(dot(point - line[0], lineVector), length_sq(lineVector));
		distSq = _mm256_add_ps(length_sq(point - (line[0] + proj * lineVector)), distSq);
	}
	return distSq;
}

template<U32 numPartitions>
FINLINE void choose_best_diagonals_RGBA8(const V4Fx8 pixels[16], V4Fx8 boundingBoxes[numPartitions], const BC7PartitionTable& table) {
	for (U32 part = 0; part < numPartitions; part++) {
		V4Fx8& min = boundingBoxes[part * 2 + 0];
		V4Fx8& max = boundingBoxes[part * 2 + 1];
		F32x8 blendMaskX = _mm256_setzero_ps();
		F32x8 blendMaskY = _mm256_setzero_ps();
		F32x8 blendMaskZ = _mm256_setzero_ps();

		// Check all 4 3d box diagonalconfigurations

		// (minx, miny, minz, minw), (maxx, maxy, maxz, maxw)
		V4Fx8 diagLine[2]{ min, max };
		F32x8 bestDistSq = pixels_dist_to_line_sq(pixels, diagLine, part, table);

		F32x8 all = _mm256_cmp_ps(_mm256_setzero_ps(), _mm256_setzero_ps(), _CMP_EQ_UQ);
		F32x8 none = _mm256_setzero_ps();

		F32x8 distSq;
		F32x8 less;
#define CHECK_DIAG(x, y, z) distSq = pixels_dist_to_line_sq(pixels, diagLine, part, table);\
		less = _mm256_cmp_ps(distSq, bestDistSq, _CMP_LT_OQ);\
		blendMaskX = _mm256_blendv_ps(blendMaskX, x, less);\
		blendMaskY = _mm256_blendv_ps(blendMaskY, y, less);\
		blendMaskZ = _mm256_blendv_ps(blendMaskZ, z, less);\
		bestDistSq = _mm256_min_ps(bestDistSq, distSq);

		// (maxx, miny, minz, minw), (minx, maxy, maxz, maxw)
		swap(&diagLine[0].x, &diagLine[1].x);
		CHECK_DIAG(all, none, none);

		// (maxx, maxy, minz, minw), (minx, miny, maxz, maxw)
		swap(&diagLine[0].y, &diagLine[1].y);
		CHECK_DIAG(all, all, none);

		// (minx, maxy, minz, minw), (maxx, miny, maxz, maxw)
		swap(&diagLine[0].x, &diagLine[1].x);
		CHECK_DIAG(none, all, none);

		// (minx, maxy, maxz, minw), (maxx, miny, minz, maxw)
		swap(&diagLine[0].z, &diagLine[1].z);
		CHECK_DIAG(none, all, all);

		// (maxx, maxy, maxz, minw), (minx, miny, minz, maxw)
		swap(&diagLine[0].x, &diagLine[1].x);
		CHECK_DIAG(all, all, all);

		// (maxx, miny, maxz, minw), (minx, maxy, minz, maxw)
		swap(&diagLine[0].y, &diagLine[1].y);
		CHECK_DIAG(all, none, all);

		// (minx, miny, maxz, minw), (maxx, maxy, minz, maxw)
		swap(&diagLine[0].x, &diagLine[1].x);
		CHECK_DIAG(none, none, all);

#undef CHECK_DIAG

		// Blend together final values, write to out.
		// Perhaps I could combine this with the above code and output values directly instead of a mask
		F32x8 x0 = _mm256_blendv_ps(min.x, max.x, blendMaskX);
		F32x8 y0 = _mm256_blendv_ps(min.y, max.y, blendMaskY);
		F32x8 z0 = _mm256_blendv_ps(min.z, max.z, blendMaskZ);
		F32x8 x1 = _mm256_blendv_ps(max.x, min.x, blendMaskX);
		F32x8 y1 = _mm256_blendv_ps(max.y, min.y, blendMaskY);
		F32x8 z1 = _mm256_blendv_ps(max.z, min.z, blendMaskZ);
		boundingBoxes[part * 2 + 0] = V4Fx8{ x0, y0, z0, min.w };
		boundingBoxes[part * 2 + 1] = V4Fx8{ x1, y1, z1, max.w };
	}
}

template<U32 numPartitions>
FINLINE void choose_best_diagonals(const V4Fx8 pixels[16], V3Fx8 boundingBoxes[numPartitions], const BC7PartitionTable& table) {
	for (U32 part = 0; part < numPartitions; part++) {
		V3Fx8& min = boundingBoxes[part * 2 + 0];
		V3Fx8& max = boundingBoxes[part * 2 + 1];
		F32x8 blendMaskX = _mm256_setzero_ps();
		F32x8 blendMaskY = _mm256_setzero_ps();

		// Check all 4 3d box diagonalconfigurations

		// (minx, miny, minz), (maxx, maxy, maxz)
		V3Fx8 diagLine[2]{ min, max };
		F32x8 bestDistSq = pixels_dist_to_line_sq(pixels, diagLine, part, table);

		F32x8 all = _mm256_cmp_ps(_mm256_setzero_ps(), _mm256_setzero_ps(), _CMP_EQ_UQ);
		F32x8 none = _mm256_setzero_ps();

		F32x8 distSq;
		F32x8 less;
#define CHECK_DIAG(x, y) distSq = pixels_dist_to_line_sq(pixels, diagLine, part, table);\
		less = _mm256_cmp_ps(distSq, bestDistSq, _CMP_LT_OQ);\
		blendMaskX = _mm256_blendv_ps(blendMaskX, x, less);\
		blendMaskY = _mm256_blendv_ps(blendMaskY, y, less);\
		bestDistSq = _mm256_min_ps(bestDistSq, distSq);

		// (maxx, miny, minz), (minx, maxy, maxz)
		swap(&diagLine[0].x, &diagLine[1].x);
		CHECK_DIAG(all, none);

		// (maxx, maxy, minz), (minx, miny, maxz)
		swap(&diagLine[0].y, &diagLine[1].y);
		CHECK_DIAG(all, all);

		// (minx, maxy, minz), (maxx, miny, maxz)
		swap(&diagLine[0].x, &diagLine[1].x);
		CHECK_DIAG(none, all);

		// Blend together final values, write to out.
		// Perhaps I could combine this with the above code and output values directly instead of a mask
		F32x8 x0 = _mm256_blendv_ps(min.x, max.x, blendMaskX);
		F32x8 y0 = _mm256_blendv_ps(min.y, max.y, blendMaskY);
		F32x8 x1 = _mm256_blendv_ps(max.x, min.x, blendMaskX);
		F32x8 y1 = _mm256_blendv_ps(max.y, min.y, blendMaskY);
		boundingBoxes[part * 2 + 0] = V3Fx8{ x0, y0, min.z };
		boundingBoxes[part * 2 + 1] = V3Fx8{ x1, y1, max.z };
	}
}

F32 quantize_bc7_endpoints3_mode0(V4F pixels[16], V3F endpoints[6], const BC7PartitionTable& table, U64* outIndices) {
	// Do most of the quantization work beforehand so we don't repeat it 4 times
	V3F preQuantizedEndpoints[6];
	for (U32 end = 0; end < 6; end++) {
		preQuantizedEndpoints[end] = truncv3f(clamp01(endpoints[end]) * 15.0F + 0.5F);
	}
	for (U32 end = 0; end < 6; end++) {
		// Ray cast against the quantized space to extend the endpoints, that way we benefit more from the precision of the interpolation
		V3F* localEndpoints = &endpoints[end & 0b110];
		U32 endpointIndex = end & 1;
		V3F endpointDirection;
		if (localEndpoints[0] == localEndpoints[1]) {
			endpointDirection = localEndpoints[0] - preQuantizedEndpoints[end] / 15.0F;
			if (end) {
				endpointDirection = -endpointDirection;
			}
		} else {
			endpointDirection = localEndpoints[endpointIndex] - localEndpoints[1 - endpointIndex];
		}
		endpointDirection = normalize(endpointDirection);

		V3F endpointScaled = (localEndpoints[endpointIndex] + 0.5F / 255.0F) * 15.0F;
		F32 t = ray_cast_unit_box(endpointScaled - truncv3f(endpointScaled), endpointDirection);
		V3F extendedEndpoint = localEndpoints[endpointIndex];
		if (endpointDirection != V3F{ 0.0F, 0.0F, 0.0F }) {
			extendedEndpoint += endpointDirection * t / 15.0F;
		}

		// 4 bits
		V3F quantized = truncv3f(clamp01(extendedEndpoint) * 15.0F + 0.5F);
		// Put bottom 3 bits in
		preQuantizedEndpoints[end] = quantized * 16.0F + truncv3f(quantized / 2.0F);
	}
	// Find best p bits to compress this endpoint
	F32 bestError[3] = { F32_LARGE, F32_LARGE, F32_LARGE };
	V3F bestQuantizedEndpoints[6];
	//U32 bestPBits[3];
	U64 bestIndices[3];
	// Try every combination of p bits and check the error each time. Not fast, but it should provide much better results for gradients where the endpoints would normally quantize to the same value
	for (U32 pBits = 0; pBits < 4; pBits++) {
		// Quantize all endpoints with this set of p bits
		V3F quantizedEndpoints[6];
		for (U32 end = 0; end < 6; end++) {
			F32 fPBit = static_cast<F32>(((pBits >> (end & 1)) & 1)) * 8.0F;
			quantizedEndpoints[end] = preQuantizedEndpoints[end] + fPBit;
		}

		// Find indices for each pixel for the quantized endpoints, and find the error for each set of endpoints
		F32 error[3] = { 0.0F, 0.0F, 0.0F };
		U64 indices[3] = { 0, 0, 0 };
		for (U32 pixel = 0; pixel < 16; pixel++) {
			U32 partition = table.partitionNumbers[pixel];
			U64 index = static_cast<U64>(find_index<3>(&quantizedEndpoints[partition * 2], v3f_xyz(pixels[pixel]) * 255.0F));
			indices[partition] |= index << (pixel * 3);
			V3F compressed{
				F32(bc7_interpolate(U8(quantizedEndpoints[partition * 2 + 0].x), U8(quantizedEndpoints[partition * 2 + 1].x), bc7InterpolationFactors3[index])) / 255.0F,
				F32(bc7_interpolate(U8(quantizedEndpoints[partition * 2 + 0].y), U8(quantizedEndpoints[partition * 2 + 1].y), bc7InterpolationFactors3[index])) / 255.0F,
				F32(bc7_interpolate(U8(quantizedEndpoints[partition * 2 + 0].z), U8(quantizedEndpoints[partition * 2 + 1].z), bc7InterpolationFactors3[index])) / 255.0F
			};
			error[partition] += length_sq(compressed - v3f_xyz(pixels[pixel]));
		}
		for (U32 partition = 0; partition < 3; partition++) {
			if (error[partition] < bestError[partition]) {
				memcpy(&bestQuantizedEndpoints[partition * 2], &quantizedEndpoints[partition * 2], 2 * sizeof(V3F));
				//bestPBits[partition] = pBits;
				bestError[partition] = error[partition];
				bestIndices[partition] = indices[partition];
			}
		}
	}
	// Write all the indices found so we don't have to recalculate them, and output the best quantized endpoints we found
	*outIndices = bestIndices[0] | bestIndices[1] | bestIndices[2];
	memcpy(endpoints, bestQuantizedEndpoints, 6 * sizeof(V3F));
	return bestError[0] + bestError[1] + bestError[2];
}

// What did the 3 mean??? I can't remember and I didn't think to comment it
F32 quantize_bc7_endpoints3_mode1(V4F pixels[16], V3F endpoints[4], const BC7PartitionTable& table, U64* outIndices) {
	// Do most of the quantization work beforehand so we don't repeat it 4 times
	V3F preQuantizedEndpoints[4];
	for (U32 end = 0; end < 4; end++) {
		preQuantizedEndpoints[end] = truncv3f(clamp01(endpoints[end]) * 63.0F + 0.5F);
	}
	for (U32 end = 0; end < 4; end++) {
		// Ray cast against the quantized space to extend the endpoints, that way we benefit more from the precision of the interpolation
		V3F* localEndpoints = &endpoints[end & 0b10];
		U32 endpointIndex = end & 1;
		V3F endpointDirection;
		if (localEndpoints[0] == localEndpoints[1]) {
			endpointDirection = localEndpoints[0] - preQuantizedEndpoints[end] / 63.0F;
			if (end) {
				endpointDirection = -endpointDirection;
			}
		} else {
			endpointDirection = localEndpoints[endpointIndex] - localEndpoints[1 - endpointIndex];
		}
		V3F normalizedEndpointDirection = normalize(endpointDirection);

		V3F endpointScaled = (localEndpoints[endpointIndex] + 0.5F / 255.0F) * 63.0F;
		F32 t = ray_cast_unit_box(endpointScaled - truncv3f(endpointScaled), normalizedEndpointDirection);
		V3F extendedEndpoint = localEndpoints[endpointIndex];
		if (endpointDirection != V3F{ 0.0F, 0.0F, 0.0F }) {
			extendedEndpoint += normalizedEndpointDirection * t / 63.0F;
		}
		// 6 bits
		V3F quantized = truncv3f(clamp01(extendedEndpoint) * 63.0F + 0.5F);
		// Put bottom bit in
		preQuantizedEndpoints[end] = quantized * 4.0F + truncv3f(quantized / 32.0F);
	}
	// Find best p bits to compress this endpoint
	F32 bestError[2] = { F32_LARGE, F32_LARGE };
	V3F bestQuantizedEndpoints[4];
	U64 bestIndices[2];
	// Try both p bits and check the error each time to get the best result. Could try optimizing this to find the p bit without brute forcing both
	for (U32 pBit = 0; pBit < 2; pBit++) {
		// Quantize all endpoints with this p bit
		V3F quantizedEndpoints[4];
		F32 fPBit = static_cast<F32>(pBit) * 2.0F;
		for (U32 end = 0; end < 4; end++) {
			quantizedEndpoints[end] = preQuantizedEndpoints[end] + fPBit;
		}

		// Find indices for each pixel for the quantized endpoints, and find the error for each set of endpoints
		F32 error[2] = { 0.0F, 0.0F };
		U64 indices[3] = { 0, 0, 0 };
		for (U32 pixel = 0; pixel < 16; pixel++) {
			U32 partition = table.partitionNumbers[pixel];
			V4F pix4 = pixels[pixel];
			V3F pix3{ pix4.x, pix4.y, pix4.z };
			U64 index = static_cast<U64>(find_index<3>(&quantizedEndpoints[partition * 2], pix3 * 255.0F));
			indices[partition] |= index << (pixel * 3);
			V3F compressed = bc7_interpolate(quantizedEndpoints[partition * 2 + 0], quantizedEndpoints[partition * 2 + 1], bc7InterpolationFactors3[index], 0) / 255.0F;
			error[partition] += length_sq(compressed - v3f_xyz(pixels[pixel]));
		}
		for (U32 partition = 0; partition < 2; partition++) {
			if (error[partition] < bestError[partition]) {
				memcpy(&bestQuantizedEndpoints[partition * 2], &quantizedEndpoints[partition * 2], 2 * sizeof(V3F));
				bestError[partition] = error[partition];
				bestIndices[partition] = indices[partition];
			}
		}
	}
	// Write all the indices found so we don't have to recalculate them, and output the best quantized endpoints we found
	*outIndices = bestIndices[0] | bestIndices[1];
	memcpy(endpoints, bestQuantizedEndpoints, 4 * sizeof(V3F));
	return bestError[0] + bestError[1];
}

template<U32 partitions, U32 componentBits, U32 numPBits, U32 indexResolution, typename Endpoint = V3F, U32 alphaBits = 0>
F32 quantize_bc7_endpoints(V4F pixels[16], Endpoint endpoints[partitions * 2], const BC7PartitionTable& table, U64* outIndices) {
	constexpr U32 numEndpoints = partitions * 2;
	constexpr F32 quantizeScaleXYZ = static_cast<F32>((1 << componentBits) - 1);
	constexpr F32 quantizeScaleAlpha = static_cast<F32>((1 << alphaBits) - 1);
	Endpoint quantizeScale; if constexpr (alphaBits > 0) { quantizeScale = Endpoint{ quantizeScaleXYZ, quantizeScaleXYZ, quantizeScaleXYZ, quantizeScaleAlpha }; } else { quantizeScale = splat<Endpoint>(quantizeScaleXYZ); };
	constexpr F32 pBitShiftXYZ = static_cast<F32>(1 << (8 - componentBits - 1));
	constexpr F32 pBitShiftAlpha = static_cast<F32>(1 << (8 - alphaBits - 1));
	Endpoint pBitShift; if constexpr (alphaBits > 0) { pBitShift = Endpoint{ pBitShiftXYZ, pBitShiftXYZ, pBitShiftXYZ, pBitShiftAlpha }; } else { pBitShift = splat<Endpoint>(pBitShiftXYZ); };
	constexpr F32 dataShiftXYZ = static_cast<F32>(1 << (8 - componentBits));
	constexpr F32 dataShiftAlpha = static_cast<F32>(1 << (8 - alphaBits));
	Endpoint dataShift; if constexpr (alphaBits > 0) { dataShift = Endpoint{ dataShiftXYZ, dataShiftXYZ, dataShiftXYZ, dataShiftAlpha }; } else { dataShift = splat<Endpoint>(dataShiftXYZ); };
	constexpr F32 bottomDataShiftXYZ = numPBits > 0 ? static_cast<F32>(1 << (componentBits - (8 - componentBits - 1))) : static_cast<F32>(1 << (componentBits - (8 - componentBits)));
	constexpr F32 bottomDataShiftAlpha = numPBits > 0 ? static_cast<F32>(1 << (alphaBits - (8 - alphaBits - 1))) : static_cast<F32>(1 << (alphaBits - (8 - alphaBits)));
	Endpoint bottomDataShift; if constexpr (alphaBits > 0) { bottomDataShift = Endpoint{ bottomDataShiftXYZ, bottomDataShiftXYZ, bottomDataShiftXYZ, bottomDataShiftAlpha }; } else { bottomDataShift = splat<Endpoint>(bottomDataShiftXYZ); };

	// Do most of the quantization work beforehand so we don't repeat it 4 times
	Endpoint preQuantizedEndpoints[numEndpoints];
	for (U32 end = 0; end < numEndpoints; end++) {
		preQuantizedEndpoints[end] = truncf(clamp01(endpoints[end]) * quantizeScale + 0.5F);
	}
	for (U32 end = 0; end < numEndpoints; end++) {
		// Ray cast against the quantized space to extend the endpoints, that way we benefit more from the precision of the interpolation
		Endpoint* localEndpoints = &endpoints[end & (~1ui32)];
		U32 endpointIndex = end & 1;
		Endpoint endpointDirection;
		if (localEndpoints[0] == localEndpoints[1]) {
			endpointDirection = localEndpoints[0] - preQuantizedEndpoints[end] / quantizeScale;
			if (end & 1) {
				endpointDirection = -endpointDirection;
			}
		} else {
			endpointDirection = localEndpoints[endpointIndex] - localEndpoints[1 - endpointIndex];
		}
		Endpoint normalizedEndpointDirection = normalize(endpointDirection);

		Endpoint endpointScaled = (localEndpoints[endpointIndex] + 0.5F / 255.0F) * quantizeScale;
		F32 raycastIntersectTime = ray_cast_unit_box(endpointScaled - truncf(endpointScaled), normalizedEndpointDirection);
		Endpoint extendedEndpoint = localEndpoints[endpointIndex];
		if (endpointDirection != Endpoint{}) {
			extendedEndpoint += normalizedEndpointDirection * raycastIntersectTime / quantizeScale;
		}
		//6 bits
		Endpoint quantized = truncf(clamp01(extendedEndpoint) * quantizeScale + 0.5F);
		// Put bottom bits in
		preQuantizedEndpoints[end] = quantized * dataShift + truncf(quantized / bottomDataShift);
	}
	// Find best p bits to compress this endpoint
	F32 bestError[partitions];
	for (U32 i = 0; i < partitions; i++) {
		bestError[i] = F32_LARGE;
	}
	Endpoint bestQuantizedEndpoints[numEndpoints];
	U64 bestIndices[3]{};
	// Try both p bits and check the error each time to get the best result. Could try optimizing this to find the p bit without brute forcing both
	for (U32 pBit = 0; pBit < (1 << numPBits); pBit++) {
		// Quantize all endpoints with this p bit
		Endpoint quantizedEndpoints[numEndpoints];
		Endpoint fPBit0 = static_cast<F32>(pBit & 0b01) * pBitShift;
		Endpoint fPBit1 = numPBits == 2 ? static_cast<F32>((pBit & 0b10) >> 1) * pBitShift : fPBit0;
		Endpoint fPBits[2]{ fPBit0, fPBit1 };
		for (U32 end = 0; end < numEndpoints; end++) {
			quantizedEndpoints[end] = preQuantizedEndpoints[end] + fPBits[end & 1];
		}

		// Find indices for each pixel for the quantized endpoints, and find the error for each set of endpoints
		F32 error[partitions]{};
		U64 indices[partitions]{};
		for (U32 pixel = 0; pixel < 16; pixel++) {
			Endpoint pixelEndpoint = convert_v4f_to_endpoint<Endpoint>(pixels[pixel]);
			U32 partition = table.partitionNumbers[pixel];
			U64 index = static_cast<U64>(find_index<indexResolution>(&quantizedEndpoints[partition * 2], pixelEndpoint * 255.0F));;
			U32 interpolationFactor;
			static_assert(indexResolution == 2 || indexResolution == 3 || indexResolution == 4, "Index resolution wrong");
			if constexpr (indexResolution == 2) {
				interpolationFactor = bc7InterpolationFactors2[index];
			} else if constexpr (indexResolution == 3) {
				interpolationFactor = bc7InterpolationFactors3[index];
			} else if constexpr (indexResolution == 4) {
				interpolationFactor = bc7InterpolationFactors4[index];
			}
			indices[partition] |= index << (pixel * indexResolution);
			Endpoint compressed = bc7_interpolate(quantizedEndpoints[partition * 2 + 0], quantizedEndpoints[partition * 2 + 1], interpolationFactor, 0) / 255.0F;
			error[partition] += length_sq(compressed - pixelEndpoint);
		}
		for (U32 partition = 0; partition < partitions; partition++) {
			if (error[partition] < bestError[partition]) {
				memcpy(&bestQuantizedEndpoints[partition * 2], &quantizedEndpoints[partition * 2], 2 * sizeof(Endpoint));
				bestError[partition] = error[partition];
				bestIndices[partition] = indices[partition];
			}
		}
	}
	// Write all the indices found so we don't have to recalculate them, and output the best quantized endpoints we found
	*outIndices = bestIndices[0] | bestIndices[1] | bestIndices[2];
	memcpy(endpoints, bestQuantizedEndpoints, numEndpoints * sizeof(Endpoint));
	F32 finalError = 0;
	for (U32 i = 0; i < partitions; i++) {
		finalError += bestError[i];
	}
	return finalError;
}

template<U32 partitions, U32 componentBits, U32 numPBits, U32 indexResolution, typename Endpoint = V3Fx8, U32 alphaBits = 0>
F32x8 quantize_bc7_endpointsx8(V4Fx8 pixels[16], Endpoint endpoints[partitions * 2], const BC7PartitionTable& table, U64x4 outIndices[2]) {
	// Bunch of constants to use in the function
	// Compiler should optimize all this stuff away
	constexpr U32 numEndpoints = partitions * 2;
	const F32x8 quantizeScaleXYZ = _mm256_set1_ps(static_cast<F32>((1 << componentBits) - 1));
	const F32x8 quantizeScaleAlpha = _mm256_set1_ps(static_cast<F32>((1 << alphaBits) - 1));
	Endpoint quantizeScale; if constexpr (alphaBits > 0) { quantizeScale = Endpoint{ quantizeScaleXYZ, quantizeScaleXYZ, quantizeScaleXYZ, quantizeScaleAlpha }; } else { quantizeScale = splat<Endpoint>(quantizeScaleXYZ); }
	Endpoint invQuantizeScale = rcp(quantizeScale);
	const F32x8 pBitShiftXYZ = _mm256_set1_ps(static_cast<F32>(1 << (8 - componentBits - 1)));
	const F32x8 pBitShiftAlpha = _mm256_set1_ps(static_cast<F32>(1 << (8 - alphaBits - 1)));
	Endpoint pBitShift; if constexpr (alphaBits > 0) { pBitShift = Endpoint{ pBitShiftXYZ, pBitShiftXYZ, pBitShiftXYZ, pBitShiftAlpha }; } else { pBitShift = splat<Endpoint>(pBitShiftXYZ); }
	const F32x8 dataShiftXYZ = _mm256_set1_ps(static_cast<F32>(1 << (8 - componentBits)));
	const F32x8 dataShiftAlpha = _mm256_set1_ps(static_cast<F32>(1 << (8 - alphaBits)));
	Endpoint dataShift; if constexpr (alphaBits > 0) { dataShift = Endpoint{ dataShiftXYZ, dataShiftXYZ, dataShiftXYZ, dataShiftAlpha }; } else { dataShift = splat<Endpoint>(dataShiftXYZ); }
	const F32x8 bottomDataShiftXYZ = _mm256_set1_ps(numPBits > 0 ? static_cast<F32>(1 << (componentBits - (8 - componentBits - 1))) : static_cast<F32>(1 << (componentBits - (8 - componentBits))));
	const F32x8 bottomDataShiftAlpha = _mm256_set1_ps(numPBits > 0 ? static_cast<F32>(1 << (alphaBits - (8 - alphaBits - 1))) : static_cast<F32>(1 << (alphaBits - (8 - alphaBits))));
	Endpoint bottomDataShift; if constexpr (alphaBits > 0) { bottomDataShift = Endpoint{ bottomDataShiftXYZ, bottomDataShiftXYZ, bottomDataShiftXYZ, bottomDataShiftAlpha }; } else { bottomDataShift = splat<Endpoint>(bottomDataShiftXYZ); }
	Endpoint invBottomDataShift = rcp(bottomDataShift);


	// Do most of the quantization work beforehand so we don't repeat it 4 times
	Endpoint preQuantizedEndpoints[numEndpoints];
	for (U32 end = 0; end < numEndpoints; end++) {
		preQuantizedEndpoints[end] = truncf(fmadd(clamp01(endpoints[end]), quantizeScale, _mm256_set1_ps(0.5F)));
	}
	for (U32 end = 0; end < numEndpoints; end++) {
		// Ray cast against the quantized space to extend the endpoints, that way we benefit more from the precision of the interpolation
		Endpoint* localEndpoints = &endpoints[end & (~1ui32)];
		U32 endpointIndex = end & 1;

		F32x8 endpointsSame = localEndpoints[0] == localEndpoints[1];
		Endpoint endpointDirectionWhenSame = sub(localEndpoints[0], mul(preQuantizedEndpoints[end], invQuantizeScale));
		if (end & 1) {
			endpointDirectionWhenSame = sub(_mm256_setzero_ps(), endpointDirectionWhenSame);
		}
		Endpoint endpointDirectionWhenDifferent = sub(localEndpoints[endpointIndex], localEndpoints[1 - endpointIndex]);
		Endpoint endpointDirection = blend(endpointDirectionWhenDifferent, endpointDirectionWhenSame, endpointsSame);
		Endpoint endpointDirectionNormalized = normalize(endpointDirection);

		Endpoint endpointScaled = mul(add(localEndpoints[endpointIndex], _mm256_set1_ps(0.5F / 255.0F)), quantizeScale);
		// This raycast *will* break with fast math turned on. Don't turn on fast math!
		F32x8 raycastIntersectTime = ray_cast_unit_box(sub(endpointScaled, truncf(endpointScaled)), endpointDirectionNormalized);
		Endpoint regularEndpoint = localEndpoints[endpointIndex];

		F32x8 endpointDirectionIsZero = equal_zero(endpointDirection);
		Endpoint endpointExtension = fmadd(mul(endpointDirectionNormalized, invQuantizeScale), raycastIntersectTime, regularEndpoint);

		Endpoint extendedEndpoint = blend(endpointExtension, regularEndpoint, endpointDirectionIsZero);
		Endpoint quantized = truncf(fmadd(clamp01(extendedEndpoint), quantizeScale, _mm256_set1_ps(0.5F)));

		// Put bottom bits in
		preQuantizedEndpoints[end] = fmadd(quantized, dataShift, truncf(mul(quantized, invBottomDataShift)));
	}

	// Find best p bits to compress this endpoint
	F32x8 bestError[partitions];
	for (U32 i = 0; i < partitions; i++) {
		bestError[i] = _mm256_set1_ps(F32_LARGE);
	}
	Endpoint bestQuantizedEndpoints[numEndpoints];
	U64x4 bestIndices[3][2]{};
	// Try both p bits and check the error each time to get the best result. Could try optimizing this to find the p bit without brute forcing both
	for (U32 pBit = 0; pBit < (1 << numPBits); pBit++) {
		// Quantize all endpoints with this p bit
		Endpoint quantizedEndpoints[numEndpoints];
		Endpoint fPBit0 = mul(_mm256_set1_ps(static_cast<F32>(pBit & 0b01)), pBitShift);
		Endpoint fPBit1 = numPBits == 2 ? mul(_mm256_set1_ps(static_cast<F32>((pBit & 0b10) >> 1)), pBitShift) : fPBit0;
		Endpoint fPBits[2]{ fPBit0, fPBit1 };
		for (U32 end = 0; end < numEndpoints; end++) {
			quantizedEndpoints[end] = add(preQuantizedEndpoints[end], fPBits[end & 1]);
		}

		// Find indices for each pixel for the quantized endpoints, and find the error for each set of endpoints
		F32x8 error[partitions];
		U64x4 indices[partitions][2];
		for (U32 i = 0; i < partitions; i++) {
			error[i] = _mm256_setzero_ps();
			indices[i][0] = _mm256_setzero_si256();
			indices[i][1] = _mm256_setzero_si256();
		}
		for (U32 pixel = 0; pixel < 16; pixel++) {
			U32 partition = table.partitionNumbers[pixel];
			Endpoint pixelEndpoint = convert_v4fx8_to_endpoint<Endpoint>(pixels[pixel]);
			U32x8 index = find_index<indexResolution>(&quantizedEndpoints[partition * 2], mul(pixelEndpoint, _mm256_set1_ps(255.0F)));
			U32x8 interpolationFactor;
			static_assert(indexResolution == 2 || indexResolution == 3 || indexResolution == 4, "Index resolution wrong");
			if constexpr (indexResolution == 2) {
				// (index * 86) >> 2, yields the bc7InterpolationFactors2 table
				// I just used linear regression to find some linear equations I could turn into integer math
				// Doing this in math is probably better than as a gather
				interpolationFactor = _mm256_srli_epi32(_mm256_mullo_epi32(index, _mm256_set1_epi32(86)), 2);
			} else if constexpr (indexResolution == 3) {
				// (index * 37) >> 2, same as above but for bc7InterpolationFactors3
				interpolationFactor = _mm256_srli_epi32(_mm256_mullo_epi32(index, _mm256_set1_epi32(37)), 2);
			} else if constexpr (indexResolution == 4) {
				// (index * 68 + 8) >> 4, same as above but for bc7InterpolationFactors4
				interpolationFactor = _mm256_srli_epi32(_mm256_add_epi32(_mm256_mullo_epi32(index, _mm256_set1_epi32(68)), _mm256_set1_epi32(8)), 4);
			}
			U64x4 indicesLow = _mm256_slli_epi64(_mm256_cvtepi32_epi64(_mm256_extracti128_si256(index, 0)), pixel * indexResolution);
			U64x4 indicesHigh = _mm256_slli_epi64(_mm256_cvtepi32_epi64(_mm256_extracti128_si256(index, 1)), pixel * indexResolution);
			indices[partition][0] = _mm256_or_si256(indices[partition][0], indicesLow);
			indices[partition][1] = _mm256_or_si256(indices[partition][1], indicesHigh);

			Endpoint compressed = mul(bc7_interpolatex8(quantizedEndpoints[partition * 2 + 0], quantizedEndpoints[partition * 2 + 1], interpolationFactor, _mm256_setzero_si256()), _mm256_set1_ps(1.0F / 255.0F));
			error[partition] = _mm256_add_ps(error[partition], length_sq(sub(compressed, pixelEndpoint)));
		}
		for (U32 partition = 0; partition < partitions; partition++) {
			F32x8 errorLessThan = _mm256_cmp_ps(error[partition], bestError[partition], _CMP_LT_OQ);
			// Set current best error
			bestError[partition] = _mm256_min_ps(bestError[partition], error[partition]);
			// Copy any better endpoints to the output
			bestQuantizedEndpoints[partition * 2 + 0] = blend(bestQuantizedEndpoints[partition * 2 + 0], quantizedEndpoints[partition * 2 + 0], errorLessThan);
			bestQuantizedEndpoints[partition * 2 + 1] = blend(bestQuantizedEndpoints[partition * 2 + 1], quantizedEndpoints[partition * 2 + 1], errorLessThan);
			// Copy any better indices to the output
			U32x8 errorLessThanimask = _mm256_castps_si256(errorLessThan);
			U32x8 indicesLowMask = _mm256_permutevar8x32_epi32(errorLessThanimask, _mm256_setr_epi32(0, 0, 1, 1, 2, 2, 3, 3));
			U32x8 indicesHighMask = _mm256_permutevar8x32_epi32(errorLessThanimask, _mm256_setr_epi32(4, 4, 5, 5, 6, 6, 7, 7));
			bestIndices[partition][0] = _mm256_blendv_epi8(bestIndices[partition][0], indices[partition][0], indicesLowMask);
			bestIndices[partition][1] = _mm256_blendv_epi8(bestIndices[partition][1], indices[partition][1], indicesHighMask);
		}
	}
	//Write all the indices found so we don't have to recalculate them, and output the best quantized endpoints we found
	outIndices[0] = _mm256_or_si256(bestIndices[0][0], _mm256_or_si256(bestIndices[1][0], bestIndices[2][0]));
	outIndices[1] = _mm256_or_si256(bestIndices[0][1], _mm256_or_si256(bestIndices[1][1], bestIndices[2][1]));
	memcpy(endpoints, bestQuantizedEndpoints, numEndpoints * sizeof(Endpoint));

	F32x8 finalError = _mm256_setzero_ps();
	for (U32 i = 0; i < partitions; i++) {
		finalError = _mm256_add_ps(finalError, bestError[i]);
	}
	return finalError;
}

void write_bc7_block_mode0(BC7Block& block, U32 bestPartition, V3F bestEndpoints[6], U64 bestIndices) {
	// mode 0
	block.data[0] = 0b1;
	block.data[1] = 0;
	block.data[0] |= bestPartition << 1;
	U64 endpointReds = 0;
	U64 endpointGreens = 0;
	U64 endpointBlues = 0;
	U64 endpointPBits = 0;
	for (U32 endpoint = 0; endpoint < 6; endpoint++) {
		U32 r = static_cast<U32>(bestEndpoints[endpoint].x);
		U32 g = static_cast<U32>(bestEndpoints[endpoint].y);
		U32 b = static_cast<U32>(bestEndpoints[endpoint].z);
		endpointReds |= ((r >> 4) & 0b1111) << (endpoint * 4);
		endpointGreens |= ((g >> 4) & 0b1111) << (endpoint * 4);
		endpointBlues |= ((b >> 4) & 0b1111) << (endpoint * 4);
		endpointPBits |= ((r >> 3) & 1) << endpoint;
	}
	block.data[0] |= endpointReds << 5;
	block.data[0] |= endpointGreens << 29;
	block.data[0] |= endpointBlues << 53;
	block.data[1] |= endpointBlues >> 11;
	block.data[1] |= endpointPBits << 13;

	U32 anchor2nd = bc7PartitionTable3Anchors2ndSubset[bestPartition];
	U32 anchor3rd = bc7PartitionTable3Anchors3rdSubset[bestPartition];

	U64 encodedIndices = 0;
	U32 shift = 0;
	for (U32 i = 0; i < 16; i++) {
		// 3 bit indices
		encodedIndices |= ((bestIndices >> (i * 3)) & 0b111) << shift;
		if (i == 0 || i == anchor2nd || i == anchor3rd) {
			shift += 2;
		} else {
			shift += 3;
		}
	}

	block.data[1] |= encodedIndices << 19;
}

void decompress_bc7_mode1(BC7Block& block, RGBA8 pixels[16]);

void write_bc7_block_mode1(BC7Block& block, U32 bestPartition, V3F bestEndpoints[6], U64 bestIndices) {
	// mode 1
	block.data[0] = 0b10;
	block.data[1] = 0;
	block.data[0] |= bestPartition << 2;
	U64 endpointReds = 0;
	U64 endpointGreens = 0;
	U64 endpointBlues = 0;
	U64 endpointPBits = 0;
	for (U32 endpoint = 0; endpoint < 4; endpoint++) {
		U32 r = static_cast<U32>(bestEndpoints[endpoint].x);
		U32 g = static_cast<U32>(bestEndpoints[endpoint].y);
		U32 b = static_cast<U32>(bestEndpoints[endpoint].z);
		endpointReds |= ((r >> 2) & 0b111111) << (endpoint * 6);
		endpointGreens |= ((g >> 2) & 0b111111) << (endpoint * 6);
		endpointBlues |= ((b >> 2) & 0b111111) << (endpoint * 6);
		endpointPBits |= ((r >> 1) & 1) << (endpoint >> 1);
	}
	block.data[0] |= endpointReds << 8;
	block.data[0] |= endpointGreens << 32;
	block.data[0] |= endpointBlues << 56;
	block.data[1] |= endpointBlues >> 8;
	block.data[1] |= endpointPBits << 16;

	U32 anchor2nd = bc7PartitionTable2Anchors2ndSubset[bestPartition];

	U64 encodedIndices = 0;
	U32 shift = 0;
	for (U32 i = 0; i < 16; i++) {
		// 3 bit indices
		encodedIndices |= ((bestIndices >> (i * 3)) & 0b111) << shift;
		if (i == 0 || i == anchor2nd) {
			shift += 2;
		} else {
			shift += 3;
		}
	}

	block.data[1] |= encodedIndices << 18;
}

template<typename Endpoint, U32 partitions, U32 indexResolution>
void check_flip_indices(Endpoint endpoints[partitions * 2], U64* outIndices, const U32 partitionTable, const BC7PartitionTable& table) {
	U64 indices = *outIndices;
	// For each subset, if the anchor most significant bit is 1, flip its partition around
	constexpr U64 indexMask = (1 << indexResolution) - 1;
	constexpr U64 highBitCheck = indexMask >> 1;

	bool shouldFlip[partitions];
	shouldFlip[0] = (indices & indexMask) > highBitCheck;
	if constexpr (partitions == 2) {
		shouldFlip[1] = ((indices >> (bc7PartitionTable2Anchors2ndSubset[partitionTable] * indexResolution)) & indexMask) > highBitCheck;
	} else if constexpr (partitions == 3) {
		shouldFlip[1] = ((indices >> (bc7PartitionTable3Anchors2ndSubset[partitionTable] * indexResolution)) & indexMask) > highBitCheck;
		shouldFlip[2] = ((indices >> (bc7PartitionTable3Anchors3rdSubset[partitionTable] * indexResolution)) & indexMask) > highBitCheck;
	}

	for (U32 i = 0; i < partitions; i++) {
		if (shouldFlip[i]) {
			swap(&endpoints[i * 2], &endpoints[i * 2 + 1]);
		}
	}
	for (U32 i = 0; i < 16; i++) {
		if (shouldFlip[table.partitionNumbers[i]]) {
			indices ^= indexMask << (i * indexResolution);
		}
	}
	*outIndices = indices;
}

template<typename Endpoint, U32 partitions, U32 indexResolution>
void check_flip_indices(Endpoint endpoints[partitions * 2], U64x4 outIndices[2], const U32x8 partitionTable, const U32x8 tableData[16]) {
	U64x4 indices[2]{ outIndices[0], outIndices[1] };
	//For each subset, if the anchor most significant bit is 1, flip its partition around
	constexpr U64 indexMask = (1 << indexResolution) - 1;
	const U64x4 indexMask4 = _mm256_set1_epi64x(indexMask);
	constexpr U32 highBitCheck = indexMask >> 1;
	const U32x4 highBitCheck4 = _mm_set1_epi32(highBitCheck);

	U32x8 shouldFlip[partitions];
	// (indices & indexMask) > highBitCheck
	U32x4 cmpA = _mm_cmpgt_epi32(cvt_int64x4_int32x4(_mm256_and_si256(indices[0], indexMask4)), highBitCheck4);
	U32x4 cmpB = _mm_cmpgt_epi32(cvt_int64x4_int32x4(_mm256_and_si256(indices[1], indexMask4)), highBitCheck4);
	shouldFlip[0] = _mm256_inserti128_si256(_mm256_castsi128_si256(cmpA), cmpB, 1);

	if constexpr (partitions == 2) {
		//shouldFlip[1] = ((indices >> (bc7PartitionTable2Anchors2ndSubset[partitionTable] * indexResolution)) & indexMask) > highBitCheck;

		// bc7PartitionTable2Anchors2ndSubset[partitionTable] * indexResolution
		U32x8 tableAnchors = _mm256_mullo_epi32(_mm256_i32gather_epi32(reinterpret_cast<const int*>(bc7PartitionTable2Anchors2ndSubset), partitionTable, 4), _mm256_set1_epi32(indexResolution));
		// Extract low and high halves, casting to 64 bit integers
		U64x4 tableAnchorsLow = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(tableAnchors));
		U64x4 tableAnchorsHigh = _mm256_cvtepu32_epi64(_mm256_extracti128_si256(tableAnchors, 1));
		// (indices >> anchor) & indexMask > highBitCheck
		cmpA = _mm_cmpgt_epi32(cvt_int64x4_int32x4(_mm256_and_si256(_mm256_srlv_epi64(indices[0], tableAnchorsLow), indexMask4)), highBitCheck4);
		cmpB = _mm_cmpgt_epi32(cvt_int64x4_int32x4(_mm256_and_si256(_mm256_srlv_epi64(indices[1], tableAnchorsHigh), indexMask4)), highBitCheck4);
		// Combine low and high halves again, we're back in 32 bits
		shouldFlip[1] = _mm256_inserti128_si256(_mm256_castsi128_si256(cmpA), cmpB, 1);
	} else if constexpr (partitions == 3) {
		//shouldFlip[1] = ((indices >> (bc7PartitionTable3Anchors2ndSubset[partitionTable] * indexResolution)) & indexMask) > highBitCheck;
		//shouldFlip[2] = ((indices >> (bc7PartitionTable3Anchors3rdSubset[partitionTable] * indexResolution)) & indexMask) > highBitCheck;

		// bc7PartitionTable3Anchors2ndSubset[partitionTable] * indexResolution
		U32x8 tableAnchors = _mm256_mullo_epi32(_mm256_i32gather_epi32(reinterpret_cast<const int*>(bc7PartitionTable3Anchors2ndSubset), partitionTable, 4), _mm256_set1_epi32(indexResolution));
		// Extract low and high halves, casting to 64 bit integers
		U64x4 tableAnchorsLow = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(tableAnchors));
		U64x4 tableAnchorsHigh = _mm256_cvtepu32_epi64(_mm256_extracti128_si256(tableAnchors, 1));
		// (indices >> anchor) & indexMask > highBitCheck
		cmpA = _mm_cmpgt_epi32(cvt_int64x4_int32x4(_mm256_and_si256(_mm256_srlv_epi64(indices[0], tableAnchorsLow), indexMask4)), highBitCheck4);
		cmpB = _mm_cmpgt_epi32(cvt_int64x4_int32x4(_mm256_and_si256(_mm256_srlv_epi64(indices[1], tableAnchorsHigh), indexMask4)), highBitCheck4);
		// Combine low and high halves again, we're back in 32 bits
		shouldFlip[1] = _mm256_inserti128_si256(_mm256_castsi128_si256(cmpA), cmpB, 1);

		// bc7PartitionTable3Anchors3rdSubset[partitionTable] * indexResolution
		tableAnchors = _mm256_mullo_epi32(_mm256_i32gather_epi32(reinterpret_cast<const int*>(bc7PartitionTable3Anchors3rdSubset), partitionTable, 4), _mm256_set1_epi32(indexResolution));
		// Extract low and high halves, casting to 64 bit integers
		tableAnchorsLow = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(tableAnchors));
		tableAnchorsHigh = _mm256_cvtepu32_epi64(_mm256_extracti128_si256(tableAnchors, 1));
		// (indices >> anchor) & indexMask > highBitCheck
		cmpA = _mm_cmpgt_epi32(cvt_int64x4_int32x4(_mm256_and_si256(_mm256_srlv_epi64(indices[0], tableAnchorsLow), indexMask4)), highBitCheck4);
		cmpB = _mm_cmpgt_epi32(cvt_int64x4_int32x4(_mm256_and_si256(_mm256_srlv_epi64(indices[1], tableAnchorsHigh), indexMask4)), highBitCheck4);
		// Combine low and high halves again, we're back in 32 bits
		shouldFlip[2] = _mm256_inserti128_si256(_mm256_castsi128_si256(cmpA), cmpB, 1);
	}

	for (U32 i = 0; i < partitions; i++) {
		Endpoint endA{ endpoints[i * 2 + 0] };
		Endpoint endB{ endpoints[i * 2 + 1] };
		// If should flip, swap the endpoints with a blend
		endpoints[i * 2 + 0] = blend(endA, endB, _mm256_castsi256_ps(shouldFlip[i]));
		endpoints[i * 2 + 1] = blend(endB, endA, _mm256_castsi256_ps(shouldFlip[i]));
	}

	for (U32 i = 0; i < 16; i++) {
		U32x8 tableRowi = tableData[i];
		// shouldFlipMask = blend(blend(shouldFlip[2], shouldlFlip[1], tableRowi == 1), shouldFlip[0], tableRowi == 0)
		// AKA shouldFlipMask = tableRowi == 0 ? shouldFlip[0] : tableRowi == 1 ? shouldFlip[1] : shouldFlip[2]
		F32x8 equals0 = _mm256_castsi256_ps(_mm256_cmpeq_epi32(tableRowi, _mm256_setzero_si256()));
		F32x8 equals1 = _mm256_castsi256_ps(_mm256_cmpeq_epi32(tableRowi, _mm256_set1_epi32(1)));
		F32x8 equals2 = _mm256_castsi256_ps(_mm256_cmpeq_epi32(tableRowi, _mm256_set1_epi32(2)));
		// equals0 ? shouldFlip[0] : 
		// equals1 ? shouldFlip[1] : 
		// equals2 ? shouldFlip[2] : 
		// zero
		U32x8 shouldFlipMask = _mm256_castps_si256(
			_mm256_blendv_ps(
				_mm256_blendv_ps(
					_mm256_blendv_ps(
						_mm256_setzero_ps(),
						_mm256_castsi256_ps(shouldFlip[2]), equals2),
					_mm256_castsi256_ps(shouldFlip[1]), equals1),
				_mm256_castsi256_ps(shouldFlip[0]), equals0));

		// If shouldFlipMask or flipMask is 0, the index won't be flipped
		U64x4 flipMask = _mm256_set1_epi64x(indexMask << (i * indexResolution));
		// indices[0] ^= flipMask & shouldFlipMask[0-3]
		indices[0] = _mm256_xor_si256(indices[0], _mm256_and_si256(flipMask, _mm256_cvtepi32_epi64(_mm256_castsi256_si128(shouldFlipMask))));
		// indices[1] ^= flipMask & shouldFlipMask[4-7]
		indices[1] = _mm256_xor_si256(indices[1], _mm256_and_si256(flipMask, _mm256_cvtepi32_epi64(_mm256_extracti128_si256(shouldFlipMask, 1))));
	}
	outIndices[0] = indices[0];
	outIndices[1] = indices[1];
}

void decompress_bc7_block(BC7Block& block, RGBA8 pixels[16]);

FINLINE void write_to_bc7_block(BC7Block& block, U64 data, U32 dataSize, U32& blockIndex, U32& dataOffset) {
	block.data[blockIndex] |= data << dataOffset;
	dataOffset += dataSize;
	if (dataOffset >= 64) {
		// This branch won't be hit more than once
		blockIndex = 1;
		dataOffset -= 64;
		block.data[1] |= data >> (dataSize - dataOffset);
	}
}

void write_bc7_block(BC7Block& block, U32 mode, U32 bestPartition, V4F bestEndpoints[6], U64 bestIndices, U64 bestIndices2 = 0, U32 indexSelection = 0, U32 rotation = 0) {
	U32 numSubsets = bc7NumSubsets[mode];
	U32 partitionBits = bc7PartitionBitCounts[mode];
	U32 rotationBits = bc7RotationBitCounts[mode];
	U32 indexSelectionBit = bc7IndexSelectionBit[mode];
	U32 colorBits = bc7ColorBits[mode];
	U32 alphaBits = bc7AlphaBits[mode];
	U32 endpointPBits = bc7EndpointPBits[mode];
	U32 sharedPBits = bc7SharedPBits[mode];
	U32 numPBits = (endpointPBits << 1) + sharedPBits;
	U32 indexBits = bc7IndexBits[mode];
	U32 indexBits2 = bc7SecondaryIndexBits[mode];

	block.data[0] = 1 << mode;
	block.data[1] = 0;
	block.data[0] |= bestPartition << (mode + 1);

	U32 dataOffset = mode + 1 + partitionBits;
	block.data[0] |= rotation << dataOffset;
	dataOffset += rotationBits;
	block.data[0] |= indexSelection << dataOffset;
	dataOffset += indexSelectionBit;

	U64 endpointRGBA8[4]{};
	U32 pBits = 0;

	U64 colorMask = (1 << colorBits) - 1;
	U64 alphaMask = (1 << alphaBits) - 1;
	U64 cutoffBits = 8 - colorBits;
	U64 alphaCutoffBits = 8 - alphaBits;
	for (U32 endpoint = 0; endpoint < (numSubsets * 2); endpoint++) {
		U32 r = static_cast<U32>(bestEndpoints[endpoint].x);
		U32 g = static_cast<U32>(bestEndpoints[endpoint].y);
		U32 b = static_cast<U32>(bestEndpoints[endpoint].z);
		U32 a = static_cast<U32>(bestEndpoints[endpoint].w);
		endpointRGBA8[0] |= ((r >> cutoffBits) & colorMask) << (endpoint * colorBits);
		endpointRGBA8[1] |= ((g >> cutoffBits) & colorMask) << (endpoint * colorBits);
		endpointRGBA8[2] |= ((b >> cutoffBits) & colorMask) << (endpoint * colorBits);
		endpointRGBA8[3] |= ((a >> alphaCutoffBits) & alphaMask) << (endpoint * alphaBits);
		if (numPBits == 2 || endpoint & numPBits) {
			U32 pShift = numPBits == 2 ? endpoint : endpoint >> 1;
			pBits |= ((r >> (cutoffBits - 1)) & 1) << pShift;
		}
	}

	U32 blockIndex = 0;
	for (U32 i = 0; i < (3 + (alphaBits > 0)); i++) {
		U32 dataSize = ((i == 3) ? alphaBits : colorBits) * numSubsets * 2;
		write_to_bc7_block(block, endpointRGBA8[i], dataSize, blockIndex, dataOffset);
	}
	if (numPBits) {
		write_to_bc7_block(block, pBits, numSubsets * numPBits, blockIndex, dataOffset);
	}


	U32 anchor2nd = (numSubsets == 3) ? bc7PartitionTable3Anchors2ndSubset[bestPartition] :
		(numSubsets == 2) ? bc7PartitionTable2Anchors2ndSubset[bestPartition] :
		U32_MAX;
	U32 anchor3rd = (numSubsets == 3) ? bc7PartitionTable3Anchors3rdSubset[bestPartition] : U32_MAX;

	U64 encodedIndices = 0;
	U32 shift = 0;
	U32 indexMask = (1 << indexBits) - 1;
	for (U32 i = 0; i < 16; i++) {
		encodedIndices |= ((bestIndices >> (i * indexBits)) & indexMask) << shift;
		if (i == 0 || i == anchor2nd || i == anchor3rd) {
			shift += indexBits - 1;
		} else {
			shift += indexBits;
		}
	}

	write_to_bc7_block(block, encodedIndices, shift, blockIndex, dataOffset);

	if (indexBits2) {
		encodedIndices = 0;
		shift = 0;
		indexMask = (1 << indexBits2) - 1;
		for (U32 i = 0; i < 16; i++) {
			encodedIndices |= ((bestIndices2 >> (i * indexBits2)) & indexMask) << shift;
			if (i == 0 || i == anchor2nd || i == anchor3rd) {
				shift += indexBits2 - 1;
			} else {
				shift += indexBits2;
			}
		}
		write_to_bc7_block(block, encodedIndices, shift, blockIndex, dataOffset);
	}
}

FINLINE void write_to_bc7_blockx8(BC7Blockx4 blocks[2], U64x4& data0, U64x4& data1, U32 dataSize, U32& blockIndex, U32& dataOffset) {
	blocks[0].data[blockIndex] = _mm256_or_si256(blocks[0].data[blockIndex], _mm256_slli_epi64(data0, dataOffset));
	blocks[1].data[blockIndex] = _mm256_or_si256(blocks[1].data[blockIndex], _mm256_slli_epi64(data1, dataOffset));
	dataOffset += dataSize;
	if (dataOffset >= 64) {
		// This branch won't be hit more than once
		blockIndex = 1;
		dataOffset -= 64;
		blocks[0].data[1] = _mm256_or_si256(blocks[0].data[1], _mm256_srli_epi64(data0, dataSize - dataOffset));
		blocks[1].data[1] = _mm256_or_si256(blocks[1].data[1], _mm256_srli_epi64(data1, dataSize - dataOffset));
	}
}

FINLINE void encode_bc7_block_indicesx8(BC7Blockx4 blocks[2], U64x4 bestIndices[2], U32 indexBits, U32 numSubsets, U32x8& anchor2nd, U32x8& anchor3rd, U32& blockIndex, U32& dataOffset) {
	U32x8 indexBitsx8 = _mm256_set1_epi32(indexBits);

	U64x4 encodedIndices[2]{ _mm256_setzero_si256(), _mm256_setzero_si256() };
	U32x8 shift = _mm256_setzero_si256();
	U64x4 indexMask = _mm256_set1_epi64x((1 << indexBits) - 1);
	for (U32 i = 0; i < 16; i++) {
		// encodedIndices |= ((bestIndices >> (i * indexBits)) & indexMask) << shift;
		U64x4 shiftLow = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(shift));
		U64x4 shiftHigh = _mm256_cvtepi32_epi64(_mm256_extracti128_si256(shift, 1));
		encodedIndices[0] = _mm256_or_si256(encodedIndices[0], _mm256_sllv_epi64(_mm256_and_si256(_mm256_srli_epi64(bestIndices[0], i * indexBits), indexMask), shiftLow));
		encodedIndices[1] = _mm256_or_si256(encodedIndices[1], _mm256_sllv_epi64(_mm256_and_si256(_mm256_srli_epi64(bestIndices[1], i * indexBits), indexMask), shiftHigh));

		// Update shift
		U32x8 ix8 = _mm256_set1_epi32(i);
		U32x8 cmp1st = _mm256_cmpeq_epi32(ix8, _mm256_setzero_si256());
		U32x8 cmp2nd = _mm256_cmpeq_epi32(ix8, anchor2nd);
		U32x8 cmp3rd = _mm256_cmpeq_epi32(ix8, anchor3rd);
		U32x8 anchor = _mm256_or_si256(_mm256_or_si256(cmp1st, cmp2nd), cmp3rd);
		U32x8 tmp = _mm256_sub_epi32(indexBitsx8, _mm256_and_si256(anchor, _mm256_set1_epi32(1)));
		shift = _mm256_add_epi32(shift, tmp);
	}

	write_to_bc7_blockx8(blocks, encodedIndices[0], encodedIndices[1], indexBits * 16 - numSubsets, blockIndex, dataOffset);
}

template<typename Endpoint, U32 mode>
void write_bc7_blockx8(BC7Blockx4 blocks[2], U32x8 bestPartition, Endpoint bestEndpoints[6], U64x4 bestIndices[2], U64x4 bestIndices2[2] = nullptr, U32x8 indexSelection = _mm256_setzero_si256(), U32x8 rotation = _mm256_setzero_si256()) {
	const U32 numSubsets = bc7NumSubsets[mode];
	const U32 partitionBits = bc7PartitionBitCounts[mode];
	const U32 rotationBits = bc7RotationBitCounts[mode];
	const U32 indexSelectionBit = bc7IndexSelectionBit[mode];
	const U32 colorBits = bc7ColorBits[mode];
	const U32 alphaBits = bc7AlphaBits[mode];
	const U32 endpointPBits = bc7EndpointPBits[mode];
	const U32 sharedPBits = bc7SharedPBits[mode];
	const U32 numPBits = (endpointPBits << 1) + sharedPBits;
	const U32 indexBits = bc7IndexBits[mode];
	const U32 indexBits2 = bc7SecondaryIndexBits[mode];

	// Write out the first easy parts: mode, partition, rotation (if present), index selection (if present)
	U32x8 startData = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(
		_mm256_set1_epi32(1 << mode),
		_mm256_slli_epi32(bestPartition, mode + 1)),
		_mm256_slli_epi32(rotation, mode + 1 + partitionBits)),
		_mm256_slli_epi32(indexSelection, mode + 1 + partitionBits + rotationBits));
	blocks[0].data[0] = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(startData));
	blocks[1].data[0] = _mm256_cvtepu32_epi64(_mm256_extracti128_si256(startData, 1));
	blocks[0].data[1] = _mm256_setzero_si256();
	blocks[1].data[1] = _mm256_setzero_si256();

	U32 blockIndex = 0;
	U32 dataOffset = mode + 1 + partitionBits + rotationBits + indexSelectionBit;

	// Brace initialization is probably good enough, but just to be sure...
	U32x8 endpointRGBA8[4]{ _mm256_setzero_si256(), _mm256_setzero_si256(), _mm256_setzero_si256(), _mm256_setzero_si256() };
	U32x8 pBits = _mm256_setzero_si256();

	// Gather the final endpoint color data in bit packed format
	U32x8 colorMask = _mm256_set1_epi32((1 << colorBits) - 1);
	U32x8 alphaMask = _mm256_set1_epi32((1 << alphaBits) - 1);
	U32 cutoffBits = 8 - colorBits;
	U32 alphaCutoffBits = 8 - alphaBits;
	for (U32 endpoint = 0; endpoint < (numSubsets * 2); endpoint++) {
		U32x8 r = _mm256_cvtps_epi32(bestEndpoints[endpoint].x);
		U32x8 g = _mm256_cvtps_epi32(bestEndpoints[endpoint].y);
		U32x8 b = _mm256_cvtps_epi32(bestEndpoints[endpoint].z);
		endpointRGBA8[0] = _mm256_or_si256(endpointRGBA8[0], _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(r, cutoffBits), colorMask), endpoint * colorBits));
		endpointRGBA8[1] = _mm256_or_si256(endpointRGBA8[1], _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(g, cutoffBits), colorMask), endpoint * colorBits));
		endpointRGBA8[2] = _mm256_or_si256(endpointRGBA8[2], _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(b, cutoffBits), colorMask), endpoint * colorBits));
		if constexpr (alphaBits != 0) {
			U32x8 a = _mm256_cvtps_epi32(bestEndpoints[endpoint].w);
			endpointRGBA8[3] = _mm256_or_si256(endpointRGBA8[3], _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(a, alphaCutoffBits), alphaMask), endpoint * alphaBits));
		}
		if (numPBits == 2 || endpoint & numPBits) {
			U32 pShift = numPBits == 2 ? endpoint : endpoint >> 1;
			pBits = _mm256_or_si256(pBits, _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(r, cutoffBits - 1), _mm256_set1_epi32(1)), pShift));
		}
	}

	// Write the endpoint data
	U32 colorFinalDataIndex = 0;
	for (U32 i = 0; i < (3 + (alphaBits > 0)); i++) {
		U32 componentSize = ((i == 3) ? alphaBits : colorBits) * numSubsets * 2;
		U64x4 endpointCompLow = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(endpointRGBA8[i]));
		U64x4 endpointCompHigh = _mm256_cvtepu32_epi64(_mm256_extracti128_si256(endpointRGBA8[i], 1));
		write_to_bc7_blockx8(blocks, endpointCompLow, endpointCompHigh, componentSize, blockIndex, dataOffset);
	}
	if constexpr (numPBits) {
		U32 pbitsSize = numSubsets * numPBits;
		U64x4 pbitsLow = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(pBits));
		U64x4 pbitsHigh = _mm256_cvtepu32_epi64(_mm256_extracti128_si256(pBits, 1));
		write_to_bc7_blockx8(blocks, pbitsLow, pbitsHigh, pbitsSize, blockIndex, dataOffset);
	}

	// Gather anchor indices
	U32x8 anchor2nd;
	U32x8 anchor3rd;
	if constexpr (numSubsets == 2) {
		anchor2nd = _mm256_i32gather_epi32(reinterpret_cast<const int*>(bc7PartitionTable2Anchors2ndSubset), bestPartition, 4);
		anchor3rd = _mm256_set1_epi32(U32_MAX);
	} else if constexpr (numSubsets == 3) {
		anchor2nd = _mm256_i32gather_epi32(reinterpret_cast<const int*>(bc7PartitionTable3Anchors2ndSubset), bestPartition, 4);
		anchor3rd = _mm256_i32gather_epi32(reinterpret_cast<const int*>(bc7PartitionTable3Anchors3rdSubset), bestPartition, 4);
	} else {
		anchor3rd = anchor2nd = _mm256_set1_epi32(U32_MAX);
	}

	// Encode indices into blocks
	encode_bc7_block_indicesx8(blocks, bestIndices, indexBits, numSubsets, anchor2nd, anchor3rd, blockIndex, dataOffset);
	if constexpr (indexBits2) {
		encode_bc7_block_indicesx8(blocks, bestIndices2, indexBits2, numSubsets, anchor2nd, anchor3rd, blockIndex, dataOffset);
	}
}

// Transforms random access partition data into something more usable by SIMD
// 16x8 bytewise transpose
// Each uint8x8 in the the tabledata contains one int for the index at that position in that partition table
FINLINE void transpose_partition_tables(U32x8 tableIndices, U32x8 tableData[16], const BC7PartitionTable tables[64]) {
	// gather all our tables
	U8x16 t0 = _mm_load_si128((U8x16*)&tables[_mm256_extract_epi32(tableIndices, 0)].partitionNumbers);
	U8x16 t1 = _mm_load_si128((U8x16*)&tables[_mm256_extract_epi32(tableIndices, 1)].partitionNumbers);
	U8x16 t2 = _mm_load_si128((U8x16*)&tables[_mm256_extract_epi32(tableIndices, 2)].partitionNumbers);
	U8x16 t3 = _mm_load_si128((U8x16*)&tables[_mm256_extract_epi32(tableIndices, 3)].partitionNumbers);
	U8x16 t4 = _mm_load_si128((U8x16*)&tables[_mm256_extract_epi32(tableIndices, 4)].partitionNumbers);
	U8x16 t5 = _mm_load_si128((U8x16*)&tables[_mm256_extract_epi32(tableIndices, 5)].partitionNumbers);
	U8x16 t6 = _mm_load_si128((U8x16*)&tables[_mm256_extract_epi32(tableIndices, 6)].partitionNumbers);
	U8x16 t7 = _mm_load_si128((U8x16*)&tables[_mm256_extract_epi32(tableIndices, 7)].partitionNumbers);

	// Transpose them to SoA

	/*
	// Do the smaller 4x4 transpose fisrt
	U8x16 transpose4x4 = _mm_setr_epi8(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);
	t0 = _mm_shuffle_epi8(t0, transpose4x4);
	t1 = _mm_shuffle_epi8(t1, transpose4x4);
	t2 = _mm_shuffle_epi8(t2, transpose4x4);
	t3 = _mm_shuffle_epi8(t3, transpose4x4);
	t4 = _mm_shuffle_epi8(t4, transpose4x4);
	t5 = _mm_shuffle_epi8(t5, transpose4x4);
	t6 = _mm_shuffle_epi8(t6, transpose4x4);
	t7 = _mm_shuffle_epi8(t7, transpose4x4);*/

	// Pair up to groups of 2 (t0-3 each contain 2 lowest bytes)
	// What do t and p stand for? I don't know man, I just needed some short temp variable names. Maybe they stand for TransPose.

	// a0 a1 c0 c1...
	U8x16 p0 = _mm_unpacklo_epi8(t0, t1);
	// a2 a3 c2 c3...
	U8x16 p1 = _mm_unpacklo_epi8(t2, t3);
	// a4 a5 c4 c5...
	U8x16 p2 = _mm_unpacklo_epi8(t4, t5);
	// a6 a7 c6 c7...
	U8x16 p3 = _mm_unpacklo_epi8(t6, t7);
	// b0 b1 d0 d1...
	U8x16 p4 = _mm_unpackhi_epi8(t0, t1);
	U8x16 p5 = _mm_unpackhi_epi8(t2, t3);
	U8x16 p6 = _mm_unpackhi_epi8(t4, t5);
	U8x16 p7 = _mm_unpackhi_epi8(t6, t7);
	// Pair up to groups of 4 (t0-1 each contain 4 lowest bytes)
	t0 = _mm_unpacklo_epi16(p0, p1);
	t1 = _mm_unpacklo_epi16(p2, p3);
	t2 = _mm_unpacklo_epi16(p4, p5);
	t3 = _mm_unpacklo_epi16(p6, p7);
	t4 = _mm_unpackhi_epi16(p0, p1);
	t5 = _mm_unpackhi_epi16(p2, p3);
	t6 = _mm_unpackhi_epi16(p4, p5);
	t7 = _mm_unpackhi_epi16(p6, p7);
	// Pair up to groups of 8 (t0 now contains all 8 low bytes, ready to be converted)
	p0 = _mm_unpacklo_epi32(t0, t1);
	p1 = _mm_unpackhi_epi32(t0, t1);
	p2 = _mm_unpacklo_epi32(t4, t5);
	p3 = _mm_unpackhi_epi32(t4, t5);
	p4 = _mm_unpacklo_epi32(t2, t3);
	p5 = _mm_unpackhi_epi32(t2, t3);
	p6 = _mm_unpacklo_epi32(t6, t7);
	p7 = _mm_unpackhi_epi32(t6, t7);

	tableData[0] = _mm256_cvtepi8_epi32(p0);
	tableData[1] = _mm256_cvtepi8_epi32(_mm_srli_si128(p0, 8));
	tableData[2] = _mm256_cvtepi8_epi32(p1);
	tableData[3] = _mm256_cvtepi8_epi32(_mm_srli_si128(p1, 8));
	tableData[4] = _mm256_cvtepi8_epi32(p2);
	tableData[5] = _mm256_cvtepi8_epi32(_mm_srli_si128(p2, 8));
	tableData[6] = _mm256_cvtepi8_epi32(p3);
	tableData[7] = _mm256_cvtepi8_epi32(_mm_srli_si128(p3, 8));
	tableData[8] = _mm256_cvtepi8_epi32(p4);
	tableData[9] = _mm256_cvtepi8_epi32(_mm_srli_si128(p4, 8));
	tableData[10] = _mm256_cvtepi8_epi32(p5);
	tableData[11] = _mm256_cvtepi8_epi32(_mm_srli_si128(p5, 8));
	tableData[12] = _mm256_cvtepi8_epi32(p6);
	tableData[13] = _mm256_cvtepi8_epi32(_mm_srli_si128(p6, 8));
	tableData[14] = _mm256_cvtepi8_epi32(p7);
	tableData[15] = _mm256_cvtepi8_epi32(_mm_srli_si128(p7, 8));
}

const bool mode0Enable = false;
const bool mode1Enable = true;
const bool mode2Enable = false;
const bool mode3Enable = false;
const bool mode4Enable = false;
const bool mode5Enable = false;
const bool mode6Enable = false;
const bool mode7Enable = false;

const bool modeEnables[]{ mode0Enable, mode1Enable, mode2Enable, mode3Enable, mode4Enable, mode5Enable, mode6Enable, mode7Enable };

template<U32 mode, typename Endpoint>
F32x8 compress_bc7_block_mode01237x8(V4Fx8 pixels[16], BC7Blockx4 blocks[2], V3Fx8* mins, V3Fx8* maxes, F32x8* alphaMins, F32x8* alphaMaxes) {
	if (!modeEnables[mode]) {
		return _mm256_set1_ps(F32_LARGE);
	}

	constexpr U32 modePartitions = bc7NumSubsets[mode];
	constexpr U32 modeComponentBits = bc7ColorBits[mode];
	// If has endpoint pbits, 2, else if has shared endpoint pbits, 1, else 0
	constexpr U32 modeNumPBits = bc7EndpointPBits[mode] * 2 + bc7SharedPBits[mode];
	constexpr U32 modeIndexResolution = bc7IndexBits[mode];

	// Check each possible partition and find the one with lowest error
	F32x8 bestError = _mm256_set1_ps(F32_LARGE);
	U32x8 bestPartition{};
	Endpoint bestEndpoints[modePartitions * 2]{};
	U64x4 bestIndices[2]{};
	// It might be much better for speed to try to find the best pattern or several patterns first with a basic distance from points to line rather than checking them all.
	constexpr const BC7PartitionTable* partitionTables = modePartitions == 2 ? bc7PartitionTable2Subsets : bc7PartitionTable3Subsets;
	constexpr U32 partitionTablesToCheck = 1 << bc7PartitionBitCounts[mode];
	for (U32 partitionTable = 0; partitionTable < partitionTablesToCheck; partitionTable++) {
		const BC7PartitionTable& table = partitionTables[partitionTable];
		// Original endpoints can be min and max
		Endpoint endpoints[modePartitions * 2];
		if constexpr (mode == 7) {
			V3Fx8 minEnd = mins[partitionTable * modePartitions + 0];
			V3Fx8 maxEnd = maxes[partitionTable * modePartitions + 0];
			endpoints[0] = Endpoint{ minEnd.x, minEnd.y, minEnd.z, alphaMins[partitionTable * modePartitions + 0] };
			endpoints[1] = Endpoint{ maxEnd.x, maxEnd.y, maxEnd.z, alphaMaxes[partitionTable * modePartitions + 0] };
			minEnd = mins[partitionTable * modePartitions + 1];
			maxEnd = maxes[partitionTable * modePartitions + 1];
			endpoints[2] = Endpoint{ minEnd.x, minEnd.y, minEnd.z, alphaMins[partitionTable * modePartitions + 1] };
			endpoints[3] = Endpoint{ maxEnd.x, maxEnd.y, maxEnd.z, alphaMaxes[partitionTable * modePartitions + 1] };

			choose_best_diagonals_RGBA8<modePartitions>(pixels, endpoints, table);
		} else {
			if constexpr (modePartitions == 1) {
				endpoints[0] = min(mins[0], mins[1]);
				endpoints[1] = max(maxes[0], maxes[1]);
			} else if constexpr (modePartitions == 2 || modePartitions == 3) {
				endpoints[0] = mins[partitionTable * modePartitions + 0];
				endpoints[1] = maxes[partitionTable * modePartitions + 0];
				endpoints[2] = mins[partitionTable * modePartitions + 1];
				endpoints[3] = maxes[partitionTable * modePartitions + 1];
			}
			if constexpr (modePartitions == 3) {
				endpoints[4] = mins[partitionTable * modePartitions + 2];
				endpoints[5] = maxes[partitionTable * modePartitions + 2];
			}

			choose_best_diagonals<modePartitions>(pixels, endpoints, table);
		}

		least_squares_optimize_endpointsx8<Endpoint, modeIndexResolution, modePartitions>(pixels, endpoints, table);

		// Figure out what our indices are going to be for the optimized endpoints
		U64x4 indices[2];
		F32x8 error = quantize_bc7_endpointsx8<modePartitions, modeComponentBits, modeNumPBits, modeIndexResolution>(pixels, endpoints, table, indices);

		// Check if the errors are the current best, and write out the best values
		F32x8 errorLessThan = _mm256_cmp_ps(error, bestError, _CMP_LT_OQ);
		U32x8 partitionTablex8 = _mm256_set1_epi32(partitionTable);
		for (U32 i = 0; i < (modePartitions * 2); i++) {
			bestEndpoints[i] = blend(bestEndpoints[i], endpoints[i], errorLessThan);
		}
		// Too much casting going on here
		// bestIndices[0] = blend(bestIndices[0], indices[0], signextend32to64(errorLessThanLowerHalf))
		bestIndices[0] = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(bestIndices[0]), _mm256_castsi256_ps(indices[0]), _mm256_castsi256_ps(_mm256_cvtepi32_epi64(_mm256_castsi256_si128(_mm256_castps_si256(errorLessThan))))));
		// bestIndices[1] = blend(bestIndices[1], indices[1], signextend32to64(errorLessThanUpperHalf))
		bestIndices[1] = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(bestIndices[1]), _mm256_castsi256_ps(indices[1]), _mm256_castsi256_ps(_mm256_cvtepi32_epi64(_mm256_extracti128_si256(_mm256_castps_si256(errorLessThan), 1)))));
		bestPartition = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(bestPartition), _mm256_castsi256_ps(partitionTablex8), errorLessThan));
		bestError = _mm256_blendv_ps(bestError, error, errorLessThan);
	}
	U32x8 tablesPacked[16];
	transpose_partition_tables(bestPartition, tablesPacked, partitionTables);
	check_flip_indices<Endpoint, modePartitions, modeIndexResolution>(bestEndpoints, bestIndices, bestPartition, tablesPacked);
	write_bc7_blockx8<Endpoint, mode>(blocks, bestPartition, bestEndpoints, bestIndices);
	return bestError;
}

F32x8 compress_bc7_block_mode4x8(V4Fx8 pixels[16], BC7Blockx4 blocks[2], V3Fx8 mins[numPartitionsFor2Subsets], V3Fx8 maxes[numPartitionsFor2Subsets], F32x8 minAlphas[numPartitionsFor2Subsets], F32x8 maxAlphas[numPartitionsFor2Subsets]) {
	if (!mode4Enable) {
		return _mm256_set1_ps(F32_LARGE);
	}
	constexpr U32 mode4Partitions = 1;
	constexpr U32 mode4ComponentBits = 5;
	constexpr U32 mode4ComponentAlphaBits = 6;
	constexpr U32 mode4PBitsPerParition = 0;
	constexpr U32 mode4IndexResolution1 = 2;
	constexpr U32 mode4IndexResolution2 = 3;

	V3Fx8 endpoints[mode4Partitions * 2]{
		min(mins[0], mins[1]),
		max(maxes[0], maxes[1])
	};
	F32x8 endpointAlphas[mode4Partitions * 2]{
		_mm256_min_ps(minAlphas[0], minAlphas[1]),
		_mm256_max_ps(maxAlphas[0], maxAlphas[1])
	};

	V3Fx8 bestEndpoints[mode4Partitions * 2];
	F32x8 bestEndpointAlphas[mode4Partitions * 2];
	U64x4 bestIndices1[2];
	U64x4 bestIndices2[2];
	F32x8 bestError = _mm256_set1_ps(F32_LARGE);
	U32x8 bestRotation;
	U32x8 bestIndexSelection;

	V3Fx8 optimizedEndpoints[mode4Partitions * 2];
	F32x8 optimizedEndpointAlphas[mode4Partitions * 2];
	U64x4 indices1[2];
	U64x4 indices2[2];
	F32x8 error;

	F32x8 cmp;
	F32x8 cmpLow;
	F32x8 cmpHigh;

	U32x8 blockOfZeros[16]{};

	// Quick and dirty code deduplication so I don't end up with a 300 line monster
#define CHECK_CONFIGURATION(idxRes1, idxRes2, indc1, indc2, idxSelect, rot) optimizedEndpoints[0] = endpoints[0];\
	optimizedEndpoints[1] = endpoints[1];\
	optimizedEndpointAlphas[0] = endpointAlphas[0];\
	optimizedEndpointAlphas[1] = endpointAlphas[1];\
	choose_best_diagonals<mode4Partitions>(pixels, optimizedEndpoints, bc7DummyPartitionTableAllZeros);\
	\
	least_squares_optimize_endpointsx8<V3Fx8, idxRes1, mode4Partitions>(pixels, optimizedEndpoints, bc7DummyPartitionTableAllZeros);\
	least_squares_optimize_endpointsx8<F32x8, idxRes2, mode4Partitions>(pixels, optimizedEndpointAlphas, bc7DummyPartitionTableAllZeros);\
	error = quantize_bc7_endpointsx8<mode4Partitions, mode4ComponentBits, mode4PBitsPerParition, idxRes1>(pixels, optimizedEndpoints, bc7DummyPartitionTableAllZeros, indc1);\
	error = _mm256_add_ps(error, quantize_bc7_endpointsx8<mode4Partitions, mode4ComponentAlphaBits, mode4PBitsPerParition, idxRes2, F32x8>(pixels, optimizedEndpointAlphas, bc7DummyPartitionTableAllZeros, indc2));\
	\
	check_flip_indices<V3Fx8, mode4Partitions, idxRes1>(optimizedEndpoints, indc1, _mm256_setzero_si256(), blockOfZeros);\
	check_flip_indices<F32x8, mode4Partitions, idxRes2>(optimizedEndpointAlphas, indc2, _mm256_setzero_si256(), blockOfZeros);\
	\
	cmp = _mm256_cmp_ps(error, bestError, _CMP_LT_OQ);\
	extract_lo_hi_masks(cmp, &cmpLow, &cmpHigh);\
	bestEndpoints[0] = blend(bestEndpoints[0], optimizedEndpoints[0], cmp);\
	bestEndpoints[1] = blend(bestEndpoints[1], optimizedEndpoints[1], cmp);\
	bestEndpointAlphas[0] = blend(bestEndpointAlphas[0], optimizedEndpointAlphas[0], cmp);\
	bestEndpointAlphas[1] = blend(bestEndpointAlphas[1], optimizedEndpointAlphas[1], cmp);\
	bestIndices1[0] = blend(bestIndices1[0], indices1[0], cmpLow);\
	bestIndices1[1] = blend(bestIndices1[1], indices1[1], cmpHigh);\
	bestIndices2[0] = blend(bestIndices2[0], indices2[0], cmpLow);\
	bestIndices2[1] = blend(bestIndices2[1], indices2[1], cmpHigh);\
	bestRotation = blend(bestRotation, _mm256_set1_epi32(rot), cmp);\
	bestError = _mm256_min_ps(bestError, error);\
	bestIndexSelection = blend(bestIndexSelection, _mm256_set1_epi32(idxSelect), cmp);


	// Unchanged rotation
	CHECK_CONFIGURATION(mode4IndexResolution1, mode4IndexResolution2, indices1, indices2, 0, 0);
	CHECK_CONFIGURATION(mode4IndexResolution2, mode4IndexResolution1, indices2, indices1, 1, 0);

	// Rotate R and A
	swap(&endpoints[0].x, &endpointAlphas[0]);
	swap(&endpoints[1].x, &endpointAlphas[1]);
	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].x, &pixels[i].w);
	}

	CHECK_CONFIGURATION(mode4IndexResolution1, mode4IndexResolution2, indices1, indices2, 0, 1);
	CHECK_CONFIGURATION(mode4IndexResolution2, mode4IndexResolution1, indices2, indices1, 1, 1);

	// Rotate G and A
	swap(&endpoints[0].x, &endpointAlphas[0]);
	swap(&endpoints[1].x, &endpointAlphas[1]);
	swap(&endpoints[0].y, &endpointAlphas[0]);
	swap(&endpoints[1].y, &endpointAlphas[1]);
	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].x, &pixels[i].w);
		swap(&pixels[i].y, &pixels[i].w);
	}

	CHECK_CONFIGURATION(mode4IndexResolution1, mode4IndexResolution2, indices1, indices2, 0, 2);
	CHECK_CONFIGURATION(mode4IndexResolution2, mode4IndexResolution1, indices2, indices1, 1, 2);

	// Rotate B and A
	swap(&endpoints[0].y, &endpointAlphas[0]);
	swap(&endpoints[1].y, &endpointAlphas[1]);
	swap(&endpoints[0].z, &endpointAlphas[0]);
	swap(&endpoints[1].z, &endpointAlphas[1]);
	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].y, &pixels[i].w);
		swap(&pixels[i].z, &pixels[i].w);
	}

	CHECK_CONFIGURATION(mode4IndexResolution1, mode4IndexResolution2, indices1, indices2, 0, 3);
	CHECK_CONFIGURATION(mode4IndexResolution2, mode4IndexResolution1, indices2, indices1, 1, 3);

	// Reset pixels after
	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].z, &pixels[i].w);
	}

#undef CHECK_CONFIGURATION

	V4Fx8 finalEndpoints[2]{
		v4fx8(bestEndpoints[0], bestEndpointAlphas[0]),
		v4fx8(bestEndpoints[1], bestEndpointAlphas[1])
	};
	write_bc7_blockx8<V4Fx8, 4>(blocks, _mm256_setzero_si256(), finalEndpoints, bestIndices1, bestIndices2, bestIndexSelection, bestRotation);

	return bestError;
}

F32x8 compress_bc7_block_mode5x8(V4Fx8 pixels[16], BC7Blockx4 blocks[2], V3Fx8 mins[numPartitionsFor2Subsets], V3Fx8 maxes[numPartitionsFor2Subsets], F32x8 minAlphas[numPartitionsFor2Subsets], F32x8 maxAlphas[numPartitionsFor2Subsets]) {
	if (!mode5Enable) {
		return _mm256_set1_ps(F32_LARGE);
	}
	constexpr U32 mode5Partitions = 1;
	constexpr U32 mode5ComponentBits = 7;
	constexpr U32 mode5ComponentAlphaBits = 8;
	constexpr U32 mode5PBitsPerParition = 0;
	constexpr U32 mode5IndexResolution = 2;

	V3Fx8 endpoints[mode5Partitions * 2]{
		min(mins[0], mins[1]),
		max(maxes[0], maxes[1])
	};
	F32x8 endpointAlphas[mode5Partitions * 2]{
		_mm256_min_ps(minAlphas[0], minAlphas[1]),
		_mm256_max_ps(maxAlphas[0], maxAlphas[1])
	};

	V3Fx8 bestEndpoints[mode5Partitions * 2];
	F32x8 bestEndpointAlphas[mode5Partitions * 2];
	U64x4 bestIndices1[2];
	U64x4 bestIndices2[2];
	F32x8 bestError = _mm256_set1_ps(F32_LARGE);
	U32x8 bestRotation;

	V3Fx8 optimizedEndpoints[mode5Partitions * 2];
	F32x8 optimizedEndpointAlphas[mode5Partitions * 2];
	U64x4 indices1[2];
	U64x4 indices2[2];
	F32x8 error;

	F32x8 cmp;
	F32x8 cmpLow;
	F32x8 cmpHigh;

	U32x8 blockOfZeros[16]{};

	// Quick and dirty code deduplication so I don't end up with a 300 line monster
#define CHECK_CONFIGURATION(rot) optimizedEndpoints[0] = endpoints[0];\
	optimizedEndpoints[1] = endpoints[1];\
	optimizedEndpointAlphas[0] = endpointAlphas[0];\
	optimizedEndpointAlphas[1] = endpointAlphas[1];\
	choose_best_diagonals<mode5Partitions>(pixels, optimizedEndpoints, bc7DummyPartitionTableAllZeros);\
	\
	least_squares_optimize_endpointsx8<V3Fx8, mode5IndexResolution, mode5Partitions>(pixels, optimizedEndpoints, bc7DummyPartitionTableAllZeros);\
	least_squares_optimize_endpointsx8<F32x8, mode5IndexResolution, mode5Partitions>(pixels, optimizedEndpointAlphas, bc7DummyPartitionTableAllZeros);\
	error = quantize_bc7_endpointsx8<mode5Partitions, mode5ComponentBits, mode5PBitsPerParition, mode5IndexResolution>(pixels, optimizedEndpoints, bc7DummyPartitionTableAllZeros, indices1);\
	error = _mm256_add_ps(error, quantize_bc7_endpointsx8<mode5Partitions, mode5ComponentAlphaBits, mode5PBitsPerParition, mode5IndexResolution, F32x8>(pixels, optimizedEndpointAlphas, bc7DummyPartitionTableAllZeros, indices2));\
	\
	check_flip_indices<V3Fx8, mode5Partitions, mode5IndexResolution>(optimizedEndpoints, indices1, _mm256_setzero_si256(), blockOfZeros);\
	check_flip_indices<F32x8, mode5Partitions, mode5IndexResolution>(optimizedEndpointAlphas, indices2, _mm256_setzero_si256(), blockOfZeros);\
	\
	cmp = _mm256_cmp_ps(error, bestError, _CMP_LT_OQ);\
	extract_lo_hi_masks(cmp, &cmpLow, &cmpHigh);\
	bestEndpoints[0] = blend(bestEndpoints[0], optimizedEndpoints[0], cmp);\
	bestEndpoints[1] = blend(bestEndpoints[1], optimizedEndpoints[1], cmp);\
	bestEndpointAlphas[0] = blend(bestEndpointAlphas[0], optimizedEndpointAlphas[0], cmp);\
	bestEndpointAlphas[1] = blend(bestEndpointAlphas[1], optimizedEndpointAlphas[1], cmp);\
	bestIndices1[0] = blend(bestIndices1[0], indices1[0], cmpLow);\
	bestIndices1[1] = blend(bestIndices1[1], indices1[1], cmpHigh);\
	bestIndices2[0] = blend(bestIndices2[0], indices2[0], cmpLow);\
	bestIndices2[1] = blend(bestIndices2[1], indices2[1], cmpHigh);\
	bestRotation = blend(bestRotation, _mm256_set1_epi32(rot), cmp);\
	bestError = _mm256_min_ps(bestError, error);\


	// Unchanged rotation
	CHECK_CONFIGURATION(0);

	// Rotate R and A
	swap(&endpoints[0].x, &endpointAlphas[0]);
	swap(&endpoints[1].x, &endpointAlphas[1]);
	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].x, &pixels[i].w);
	}

	CHECK_CONFIGURATION(1);

	// Rotate G and A
	swap(&endpoints[0].x, &endpointAlphas[0]);
	swap(&endpoints[1].x, &endpointAlphas[1]);
	swap(&endpoints[0].y, &endpointAlphas[0]);
	swap(&endpoints[1].y, &endpointAlphas[1]);
	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].x, &pixels[i].w);
		swap(&pixels[i].y, &pixels[i].w);
	}

	CHECK_CONFIGURATION(2);

	// Rotate B and A
	swap(&endpoints[0].y, &endpointAlphas[0]);
	swap(&endpoints[1].y, &endpointAlphas[1]);
	swap(&endpoints[0].z, &endpointAlphas[0]);
	swap(&endpoints[1].z, &endpointAlphas[1]);
	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].y, &pixels[i].w);
		swap(&pixels[i].z, &pixels[i].w);
	}

	CHECK_CONFIGURATION(3);

	// Reset pixels after
	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].z, &pixels[i].w);
	}

#undef CHECK_CONFIGURATION

	V4Fx8 finalEndpoints[2]{
		v4fx8(bestEndpoints[0], bestEndpointAlphas[0]),
		v4fx8(bestEndpoints[1], bestEndpointAlphas[1])
	};
	write_bc7_blockx8<V4Fx8, 5>(blocks, _mm256_setzero_si256(), finalEndpoints, bestIndices1, bestIndices2, _mm256_setzero_si256(), bestRotation);
	return bestError;
}

F32x8 compress_bc7_block_mode6x8(V4Fx8 pixels[16], BC7Blockx4 blocks[2], V3Fx8 mins[numPartitionsFor2Subsets], V3Fx8 maxes[numPartitionsFor2Subsets], F32x8 minAlphas[numPartitionsFor2Subsets], F32x8 maxAlphas[numPartitionsFor2Subsets]) {
	if (!mode6Enable) {
		return _mm256_set1_ps(F32_LARGE);
	}
	constexpr U32 mode6Partitions = 1;
	constexpr U32 mode6ComponentBits = 7;
	constexpr U32 mode6ComponentAlphaBits = 7;
	constexpr U32 mode6PBitsPerParition = 2;
	constexpr U32 mode6IndexResolution = 4;

	V4Fx8 endpoints[mode6Partitions * 2]{
		v4fx8(min(mins[0], mins[1]), _mm256_min_ps(minAlphas[0], minAlphas[1])),
		v4fx8(max(maxes[0], maxes[1]), _mm256_max_ps(maxAlphas[0], maxAlphas[1]))
	};

	U64x4 indices[2];
	//choose_best_diagonals<mode6Partitions>(pixels, endpoints, bc7DummyPartitionTableAllZeros);
	least_squares_optimize_endpointsx8<V4Fx8, mode6IndexResolution, mode6Partitions>(pixels, endpoints, bc7DummyPartitionTableAllZeros);
	F32x8 error = quantize_bc7_endpointsx8<mode6Partitions, mode6ComponentBits, mode6PBitsPerParition, mode6IndexResolution>(pixels, endpoints, bc7DummyPartitionTableAllZeros, indices);
	U32x8 blockOfZeros[16]{};
	check_flip_indices<V4Fx8, mode6Partitions, mode6IndexResolution>(endpoints, indices, _mm256_setzero_si256(), blockOfZeros);
	write_bc7_blockx8<V4Fx8, 6>(blocks, _mm256_setzero_si256(), endpoints, indices);
	return error;
}

F32 compress_bc7_block_mode0(V4F pixels[16], BC7Block& block, V3F mins[numPartitionsFor3Subsets], V3F maxes[numPartitionsFor3Subsets], V3F means[numPartitionsFor3Subsets]) {
	if (!mode0Enable) {
		return F32_LARGE;
	}
	constexpr U32 mode0Partitions = 3;
	constexpr U32 mode0ComponentBits = 4;
	constexpr U32 mode0NumPBits = 2;
	constexpr U32 mode0IndexResolution = 3;

	// Check each possible partition and find the one with lowest error
	F32 bestError = F32_LARGE;
	U32 bestPartition;
	V4F bestEndpoints[6];
	U64 bestIndices;
	// Mode 0 only gets 4 bits of partition table index instead of the full 6, so shift the number right by 2.
	// It might be much better for speed to try to find the best pattern or several patterns first with a basic distance from points to line rather than checking them all.
	for (U32 partitionTable = 0; partitionTable < (numPartitionTablesPerSubset >> 2); partitionTable++) {
		const BC7PartitionTable& table = bc7PartitionTable3Subsets[partitionTable];
		// Original endpoints can be min and max
		V3F endpoints[mode0Partitions * 2]{
			mins[partitionTable * mode0Partitions + 0],
			maxes[partitionTable * mode0Partitions + 0],
			mins[partitionTable * mode0Partitions + 1],
			maxes[partitionTable * mode0Partitions + 1],
			mins[partitionTable * mode0Partitions + 2],
			maxes[partitionTable * mode0Partitions + 2]
		};
		// I didn't think it was at first, but this is actually required for the degenerate case where the points all lie on opposing diagonals, projecting to the same point on the original diagonal (which results in least squares erroring).
		choose_best_diagonals<mode0Partitions>(pixels, endpoints, table);
		// Mode 0 has 3 partitions
		for (U32 partition = 0; partition < mode0Partitions; partition++) {
			//Find indices from the original endpoints and optimize with a least squares iteration to get the new endpoints
			least_squares_optimize_endpoints_rgb<mode0IndexResolution>(pixels, &endpoints[partition * 2], table, partition);
		}

		U64 indices;
		//F32 error = quantize_bc7_endpoints3_mode0(pixels, endpoints, table, &indices);
		F32 error = quantize_bc7_endpoints<mode0Partitions, mode0ComponentBits, mode0NumPBits, mode0IndexResolution>(pixels, endpoints, table, &indices);

		// Test this partition table's error, see if it's the best one yet
		//F32 error = error_mode0(pixels, partitionTable, endpoints, indices);
		if (error < bestError) {
			check_flip_indices<V3F, 3, 3>(endpoints, &indices, partitionTable, table);
			for (U32 i = 0; i < (mode0Partitions * 2); i++) {
				bestEndpoints[i] = V4F{ endpoints[i].x, endpoints[i].y, endpoints[i].z, 0.0F };
			}
			bestIndices = indices;
			bestPartition = partitionTable;
			bestError = error;
		}
	}
	write_bc7_block(block, 0, bestPartition, bestEndpoints, bestIndices);
	return bestError;
}

F32 compress_bc7_block_mode1(V4F pixels[16], BC7Block& block, V3F mins[numPartitionsFor2Subsets], V3F maxes[numPartitionsFor2Subsets], V3F means[numPartitionsFor2Subsets]) {
	if (!mode1Enable) {
		return F32_LARGE;
	}

	constexpr U32 mode1Partitions = 2;
	constexpr U32 mode1ComponentBits = 6;
	constexpr U32 mode1NumPBits = 1;
	constexpr U32 mode1IndexResolution = 3;

	// Check each possible partition and find the one with lowest error
	F32 bestError = F32_LARGE;
	U32 bestPartition;
	V4F bestEndpoints[4];
	U64 bestIndices;
	// It might be much better for speed to try to find the best pattern or several patterns first with a basic distance from points to line rather than checking them all.
	for (U32 partitionTable = 0; partitionTable < numPartitionTablesPerSubset; partitionTable++) {
		const BC7PartitionTable& table = bc7PartitionTable2Subsets[partitionTable];
		// Original endpoints can be min and max
		V3F endpoints[mode1Partitions * 2]{
			mins[partitionTable * mode1Partitions + 0],
			maxes[partitionTable * mode1Partitions + 0],
			mins[partitionTable * mode1Partitions + 1],
			maxes[partitionTable * mode1Partitions + 1]
		};
		// I didn't think it was at first, but this is actually required for the degenerate case where the points all lie on opposing diagonals, projecting to the same point on the original diagonal (which results in least squares erroring).
		choose_best_diagonals<mode1Partitions>(pixels, endpoints, table);
		// Mode 1 has 2 partitions
		for (U32 partition = 0; partition < mode1Partitions; partition++) {
			// Find indices from the original endpoints and optimize with a least squares iteration to get the new endpoints
			least_squares_optimize_endpoints_rgb<mode1IndexResolution>(pixels, &endpoints[partition * 2], table, partition);
		}

		U64 indices;
		// F32 error = quantize_bc7_endpoints3_mode1(pixels, endpoints, table, &indices);
		F32 error = quantize_bc7_endpoints<mode1Partitions, mode1ComponentBits, mode1NumPBits, mode1IndexResolution>(pixels, endpoints, table, &indices);

		// Test this partition table's error, see if it's the best one yet
		// F32 error = error_mode0(pixels, partitionTable, endpoints, indices);
		if (error < bestError) {
			check_flip_indices<V3F, 2, 3>(endpoints, &indices, partitionTable, table);
			for (U32 i = 0; i < (mode1Partitions * 2); i++) {
				bestEndpoints[i] = V4F{ endpoints[i].x, endpoints[i].y, endpoints[i].z, 0.0F };
			}
			bestIndices = indices;
			bestPartition = partitionTable;
			bestError = error;
		}
	}
	write_bc7_block(block, 1, bestPartition, bestEndpoints, bestIndices);
	return bestError;
}

F32 compress_bc7_block_mode2(V4F pixels[16], BC7Block& block, V3F mins[numPartitionsFor2Subsets], V3F maxes[numPartitionsFor2Subsets], V3F means[numPartitionsFor2Subsets]) {
	if (!mode2Enable) {
		return F32_LARGE;
	}
	constexpr U32 mode2Partitions = 3;
	constexpr U32 mode2ComponentBits = 5;
	constexpr U32 mode2PBitsPerParition = 0;
	constexpr U32 mode2IndexResolution = 2;

	// Check each possible partition and find the one with lowest error
	F32 bestError = F32_LARGE;
	U32 bestPartition;
	V4F bestEndpoints[mode2Partitions * 2];
	U64 bestIndices;
	// It might be much better for speed to try to find the best pattern or several patterns first with a basic distance from points to line rather than checking them all.
	for (U32 partitionTable = 0; partitionTable < numPartitionTablesPerSubset; partitionTable++) {
		const BC7PartitionTable& table = bc7PartitionTable3Subsets[partitionTable];
		//Original endpoints can be min and max
		V3F endpoints[mode2Partitions * 2]{
			mins[partitionTable * mode2Partitions + 0],
			maxes[partitionTable * mode2Partitions + 0],
			mins[partitionTable * mode2Partitions + 1],
			maxes[partitionTable * mode2Partitions + 1],
			mins[partitionTable * mode2Partitions + 2],
			maxes[partitionTable * mode2Partitions + 2]
		};
		// I didn't think it was at first, but this is actually required for the degenerate case where the points all lie on opposing diagonals, projecting to the same point on the original diagonal (which results in least squares erroring).
		choose_best_diagonals<mode2Partitions>(pixels, endpoints, table);
		// Mode 1 has 2 partitions
		for (U32 partition = 0; partition < mode2Partitions; partition++) {
			// Find indices from the original endpoints and optimize with a least squares iteration to get the new endpoints
			least_squares_optimize_endpoints_rgb<mode2IndexResolution>(pixels, &endpoints[partition * 2], table, partition);
		}

		U64 indices;
		F32 error = quantize_bc7_endpoints<mode2Partitions, mode2ComponentBits, mode2PBitsPerParition, mode2IndexResolution>(pixels, endpoints, table, &indices);

		// Test this partition table's error, see if it's the best one yet
		if (error < bestError) {
			check_flip_indices<V3F, mode2Partitions, mode2IndexResolution>(endpoints, &indices, partitionTable, table);
			for (U32 i = 0; i < (mode2Partitions * 2); i++) {
				bestEndpoints[i] = V4F{ endpoints[i].x, endpoints[i].y, endpoints[i].z, 0.0F };
			}
			bestIndices = indices;
			bestPartition = partitionTable;
			bestError = error;
		}
	}
	write_bc7_block(block, 2, bestPartition, bestEndpoints, bestIndices);
	return bestError;
}

F32 compress_bc7_block_mode3(V4F pixels[16], BC7Block& block, V3F mins[numPartitionsFor2Subsets], V3F maxes[numPartitionsFor2Subsets], V3F means[numPartitionsFor2Subsets]) {
	if (!mode3Enable) {
		return F32_LARGE;
	}
	constexpr U32 mode3Partitions = 2;
	constexpr U32 mode3ComponentBits = 7;
	constexpr U32 mode3PBitsPerParition = 2;
	constexpr U32 mode3IndexResolution = 2;

	// Check each possible partition and find the one with lowest error
	F32 bestError = F32_LARGE;
	U32 bestPartition;
	V4F bestEndpoints[mode3Partitions * 2];
	U64 bestIndices;
	// It might be much better for speed to try to find the best pattern or several patterns first with a basic distance from points to line rather than checking them all.
	for (U32 partitionTable = 0; partitionTable < numPartitionTablesPerSubset; partitionTable++) {
		const BC7PartitionTable& table = bc7PartitionTable2Subsets[partitionTable];
		//Original endpoints can be min and max
		V3F endpoints[mode3Partitions * 2]{
			mins[partitionTable * mode3Partitions + 0],
			maxes[partitionTable * mode3Partitions + 0],
			mins[partitionTable * mode3Partitions + 1],
			maxes[partitionTable * mode3Partitions + 1]
		};
		// I didn't think it was at first, but this is actually required for the degenerate case where the points all lie on opposing diagonals, projecting to the same point on the original diagonal (which results in least squares erroring).
		choose_best_diagonals<mode3Partitions>(pixels, endpoints, table);
		// Mode 1 has 2 partitions
		for (U32 partition = 0; partition < mode3Partitions; partition++) {
			// Find indices from the original endpoints and optimize with a least squares iteration to get the new endpoints
			least_squares_optimize_endpoints_rgb<mode3IndexResolution>(pixels, &endpoints[partition * 2], table, partition);
		}

		U64 indices;
		F32 error = quantize_bc7_endpoints<mode3Partitions, mode3ComponentBits, mode3PBitsPerParition, mode3IndexResolution>(pixels, endpoints, table, &indices);

		// Test this partition table's error, see if it's the best one yet
		if (error < bestError) {
			check_flip_indices<V3F, mode3Partitions, mode3IndexResolution>(endpoints, &indices, partitionTable, table);
			for (U32 i = 0; i < (mode3Partitions * 2); i++) {
				bestEndpoints[i] = V4F{ endpoints[i].x, endpoints[i].y, endpoints[i].z, 0.0F };
			}
			bestIndices = indices;
			bestPartition = partitionTable;
			bestError = error;
		}
	}
	write_bc7_block(block, 3, bestPartition, bestEndpoints, bestIndices);
	return bestError;
}

F32 compress_bc7_block_mode4(V4F pixels[16], BC7Block& block, V3F mins[numPartitionsFor2Subsets], V3F maxes[numPartitionsFor2Subsets], F32 minAlphas[numPartitionsFor2Subsets], F32 maxAlphas[numPartitionsFor2Subsets]) {
	if (!mode4Enable) {
		return F32_LARGE;
	}
	constexpr U32 mode4Partitions = 1;
	constexpr U32 mode4ComponentBits = 5;
	constexpr U32 mode4ComponentAlphaBits = 6;
	constexpr U32 mode4PBitsPerParition = 0;
	constexpr U32 mode4IndexResolution1 = 2;
	constexpr U32 mode4IndexResolution2 = 3;

	V3F endpointsA[mode4Partitions * 2]{
		min(mins[0], mins[1]),
		max(maxes[0], maxes[1])
	};
	F32 endpointAlphasA[mode4Partitions * 2]{
		min(minAlphas[0], minAlphas[1]),
		max(maxAlphas[0], maxAlphas[1])
	};
	choose_best_diagonals<mode4Partitions>(pixels, endpointsA, bc7DummyPartitionTableAllZeros);
	V3F endpointsB[mode4Partitions * 2]{ endpointsA[0], endpointsA[1] };
	F32 endpointAlphasB[mode4Partitions * 2]{ endpointAlphasA[0], endpointAlphasA[1] };

	// Only two variations to try here, I'm not going to turn it into a whole loop.
	U64 indicesA;
	U64 indicesAlphaA;
	least_squares_optimize_endpoints_rgb<mode4IndexResolution1>(pixels, endpointsA, bc7DummyPartitionTableAllZeros, 0);
	least_squares_optmize_endpoints_alpha<mode4IndexResolution2>(pixels, endpointAlphasA);
	F32 errorA = quantize_bc7_endpoints<mode4Partitions, mode4ComponentBits, mode4PBitsPerParition, mode4IndexResolution1>(pixels, endpointsA, bc7DummyPartitionTableAllZeros, &indicesA);
	errorA += quantize_bc7_endpoints<mode4Partitions, mode4ComponentAlphaBits, mode4PBitsPerParition, mode4IndexResolution2, F32>(pixels, endpointAlphasA, bc7DummyPartitionTableAllZeros, &indicesAlphaA);

	U64 indicesB;
	U64 indicesAlphaB;
	least_squares_optimize_endpoints_rgb<mode4IndexResolution2>(pixels, endpointsB, bc7DummyPartitionTableAllZeros, 0);
	least_squares_optmize_endpoints_alpha<mode4IndexResolution1>(pixels, endpointAlphasB);
	F32 errorB = quantize_bc7_endpoints<mode4Partitions, mode4ComponentBits, mode4PBitsPerParition, mode4IndexResolution2>(pixels, endpointsB, bc7DummyPartitionTableAllZeros, &indicesB);
	errorB += quantize_bc7_endpoints<mode4Partitions, mode4ComponentAlphaBits, mode4PBitsPerParition, mode4IndexResolution1, F32>(pixels, endpointAlphasB, bc7DummyPartitionTableAllZeros, &indicesAlphaB);

	if (errorA < errorB) {
		check_flip_indices<V3F, mode4Partitions, mode4IndexResolution1>(endpointsA, &indicesA, 0, bc7DummyPartitionTableAllZeros);
		check_flip_indices<F32, mode4Partitions, mode4IndexResolution2>(endpointAlphasA, &indicesAlphaA, 0, bc7DummyPartitionTableAllZeros);
		V4F bestEndpoints[2]{
			V4F{ endpointsA[0].x, endpointsA[0].y, endpointsA[0].z, endpointAlphasA[0] },
			V4F{ endpointsA[1].x, endpointsA[1].y, endpointsA[1].z, endpointAlphasA[1] }
		};
		write_bc7_block(block, 4, 0, bestEndpoints, indicesA, indicesAlphaA, 0, 0);
		return errorA;
	} else {
		check_flip_indices<V3F, mode4Partitions, mode4IndexResolution2>(endpointsB, &indicesB, 0, bc7DummyPartitionTableAllZeros);
		check_flip_indices<F32, mode4Partitions, mode4IndexResolution1>(endpointAlphasB, &indicesAlphaB, 0, bc7DummyPartitionTableAllZeros);
		V4F bestEndpoints[2]{
			V4F{ endpointsB[0].x, endpointsB[0].y, endpointsB[0].z, endpointAlphasB[0] },
			V4F{ endpointsB[1].x, endpointsB[1].y, endpointsB[1].z, endpointAlphasB[1] }
		};
		write_bc7_block(block, 4, 0, bestEndpoints, indicesAlphaB, indicesB, 1, 0);
		return errorB;
	}
}

F32 compress_bc7_block_mode5(V4F pixels[16], BC7Block& block, V3F mins[numPartitionsFor2Subsets], V3F maxes[numPartitionsFor2Subsets], F32 minAlphas[numPartitionsFor2Subsets], F32 maxAlphas[numPartitionsFor2Subsets]) {
	if (!mode5Enable) {
		return F32_LARGE;
	}
	constexpr U32 mode5Partitions = 1;
	constexpr U32 mode5ComponentBits = 7;
	constexpr U32 mode5ComponentAlphaBits = 8;
	constexpr U32 mode5PBitsPerParition = 0;
	constexpr U32 mode5IndexResolution = 2;

	V3F bestEndpoints[mode5Partitions * 2]{
		min(mins[0], mins[1]),
		max(maxes[0], maxes[1])
	};
	F32 bestEndpointAlphas[mode5Partitions * 2]{
		min(minAlphas[0], minAlphas[1]),
		max(maxAlphas[0], maxAlphas[1])
	};
	V3F bestEndpointsRotR[mode5Partitions * 2]{ bestEndpoints[0], bestEndpoints[1] };
	F32 bestEndpointAlphasRotR[mode5Partitions * 2]{ bestEndpointAlphas[0], bestEndpointAlphas[1] };
	V3F bestEndpointsRotG[mode5Partitions * 2]{ bestEndpoints[0], bestEndpoints[1] };
	F32 bestEndpointAlphasRotG[mode5Partitions * 2]{ bestEndpointAlphas[0], bestEndpointAlphas[1] };
	V3F bestEndpointsRotB[mode5Partitions * 2]{ bestEndpoints[0], bestEndpoints[1] };
	F32 bestEndpointAlphasRotB[mode5Partitions * 2]{ bestEndpointAlphas[0], bestEndpointAlphas[1] };
	for (U32 i = 0; i < (mode5Partitions * 2); i++) {
		swap(&bestEndpointsRotR[i].x, &bestEndpointAlphasRotR[i]);
		swap(&bestEndpointsRotG[i].y, &bestEndpointAlphasRotG[i]);
		swap(&bestEndpointsRotB[i].z, &bestEndpointAlphasRotB[i]);
	}

	choose_best_diagonals<mode5Partitions>(pixels, bestEndpoints, bc7DummyPartitionTableAllZeros);

	U32 rotationBits = 0;
	U64 bestIndices;
	U64 bestIndicesAlpha;
	least_squares_optimize_endpoints_rgb<mode5IndexResolution>(pixels, bestEndpoints, bc7DummyPartitionTableAllZeros, 0);
	least_squares_optmize_endpoints_alpha<mode5IndexResolution>(pixels, bestEndpointAlphas);
	F32 bestError = quantize_bc7_endpoints<mode5Partitions, mode5ComponentBits, mode5PBitsPerParition, mode5IndexResolution>(pixels, bestEndpoints, bc7DummyPartitionTableAllZeros, &bestIndices);
	bestError += quantize_bc7_endpoints<mode5Partitions, mode5ComponentAlphaBits, mode5PBitsPerParition, mode5IndexResolution, F32>(pixels, bestEndpointAlphas, bc7DummyPartitionTableAllZeros, &bestIndicesAlpha);

	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].x, &pixels[i].w);
	}
	choose_best_diagonals<mode5Partitions>(pixels, bestEndpointsRotR, bc7DummyPartitionTableAllZeros);
	U64 indices;
	U64 indicesAlpha;
	least_squares_optimize_endpoints_rgb<mode5IndexResolution>(pixels, bestEndpointsRotR, bc7DummyPartitionTableAllZeros, 0);
	least_squares_optmize_endpoints_alpha<mode5IndexResolution>(pixels, bestEndpointAlphasRotR);
	F32 error = quantize_bc7_endpoints<mode5Partitions, mode5ComponentBits, mode5PBitsPerParition, mode5IndexResolution>(pixels, bestEndpointsRotR, bc7DummyPartitionTableAllZeros, &indices);
	error += quantize_bc7_endpoints<mode5Partitions, mode5ComponentAlphaBits, mode5PBitsPerParition, mode5IndexResolution, F32>(pixels, bestEndpointAlphasRotR, bc7DummyPartitionTableAllZeros, &indicesAlpha);
	if (error < bestError) {
		bestError = error;
		bestIndices = indices;
		bestIndicesAlpha = indicesAlpha;
		memcpy(bestEndpoints, bestEndpointsRotR, 2 * sizeof(V3F));
		memcpy(bestEndpointAlphas, bestEndpointAlphasRotR, 2 * sizeof(F32));
		rotationBits = 1;
	}

	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].x, &pixels[i].w);
		swap(&pixels[i].y, &pixels[i].w);
	}
	choose_best_diagonals<mode5Partitions>(pixels, bestEndpointsRotG, bc7DummyPartitionTableAllZeros);
	least_squares_optimize_endpoints_rgb<mode5IndexResolution>(pixels, bestEndpointsRotG, bc7DummyPartitionTableAllZeros, 0);
	least_squares_optmize_endpoints_alpha<mode5IndexResolution>(pixels, bestEndpointAlphasRotG);
	error = quantize_bc7_endpoints<mode5Partitions, mode5ComponentBits, mode5PBitsPerParition, mode5IndexResolution>(pixels, bestEndpointsRotG, bc7DummyPartitionTableAllZeros, &indices);
	error += quantize_bc7_endpoints<mode5Partitions, mode5ComponentAlphaBits, mode5PBitsPerParition, mode5IndexResolution, F32>(pixels, bestEndpointAlphasRotG, bc7DummyPartitionTableAllZeros, &indicesAlpha);
	if (error < bestError) {
		bestError = error;
		bestIndices = indices;
		bestIndicesAlpha = indicesAlpha;
		memcpy(bestEndpoints, bestEndpointsRotG, 2 * sizeof(V3F));
		memcpy(bestEndpointAlphas, bestEndpointAlphasRotG, 2 * sizeof(F32));
		rotationBits = 2;
	}

	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].y, &pixels[i].w);
		swap(&pixels[i].z, &pixels[i].w);
	}
	choose_best_diagonals<mode5Partitions>(pixels, bestEndpointsRotB, bc7DummyPartitionTableAllZeros);
	least_squares_optimize_endpoints_rgb<mode5IndexResolution>(pixels, bestEndpointsRotB, bc7DummyPartitionTableAllZeros, 0);
	least_squares_optmize_endpoints_alpha<mode5IndexResolution>(pixels, bestEndpointAlphasRotB);
	error = quantize_bc7_endpoints<mode5Partitions, mode5ComponentBits, mode5PBitsPerParition, mode5IndexResolution>(pixels, bestEndpointsRotB, bc7DummyPartitionTableAllZeros, &indices);
	error += quantize_bc7_endpoints<mode5Partitions, mode5ComponentAlphaBits, mode5PBitsPerParition, mode5IndexResolution, F32>(pixels, bestEndpointAlphasRotB, bc7DummyPartitionTableAllZeros, &indicesAlpha);
	if (error < bestError) {
		bestError = error;
		bestIndices = indices;
		bestIndicesAlpha = indicesAlpha;
		memcpy(bestEndpoints, bestEndpointsRotB, 2 * sizeof(V3F));
		memcpy(bestEndpointAlphas, bestEndpointAlphasRotB, 2 * sizeof(F32));
		rotationBits = 3;
	}

	for (U32 i = 0; i < 16; i++) {
		swap(&pixels[i].z, &pixels[i].w);
	}

	check_flip_indices<V3F, mode5Partitions, mode5IndexResolution>(bestEndpoints, &bestIndices, 0, bc7DummyPartitionTableAllZeros);
	check_flip_indices<F32, mode5Partitions, mode5IndexResolution>(bestEndpointAlphas, &bestIndicesAlpha, 0, bc7DummyPartitionTableAllZeros);
	V4F bestEndpointsRGBA8[2]{
		V4F{ bestEndpoints[0].x, bestEndpoints[0].y, bestEndpoints[0].z, bestEndpointAlphas[0] },
		V4F{ bestEndpoints[1].x, bestEndpoints[1].y, bestEndpoints[1].z, bestEndpointAlphas[1] }
	};
	write_bc7_block(block, 5, 0, bestEndpointsRGBA8, bestIndices, bestIndicesAlpha, 0, rotationBits);
	return bestError;
}

F32 compress_bc7_block_mode6(V4F pixels[16], BC7Block& block, V3F mins[numPartitionsFor2Subsets], V3F maxes[numPartitionsFor2Subsets], F32 minAlphas[numPartitionsFor2Subsets], F32 maxAlphas[numPartitionsFor2Subsets]) {
	if (!mode6Enable) {
		return F32_LARGE;
	}
	constexpr U32 mode6Partitions = 1;
	constexpr U32 mode6ComponentBits = 7;
	constexpr U32 mode6PBitsPerParition = 2;
	constexpr U32 mode6IndexResolution = 4;

	V4F endpoints[mode6Partitions * 2]{
		v4f(min(mins[0], mins[1]), min(minAlphas[0], minAlphas[1])),
		v4f(max(maxes[0], maxes[1]), max(maxAlphas[0], maxAlphas[1]))
	};
	choose_best_diagonals_RGBA8<mode6Partitions>(pixels, endpoints, bc7DummyPartitionTableAllZeros);

	// Only two variations to try here, I'm not going to turn it into a whole loop.
	U64 indices;
	least_squares_optimize_endpoints_RGBA8<mode6IndexResolution>(pixels, endpoints, bc7DummyPartitionTableAllZeros, 0);
	F32 error = quantize_bc7_endpoints<mode6Partitions, mode6ComponentBits, mode6PBitsPerParition, mode6IndexResolution>(pixels, endpoints, bc7DummyPartitionTableAllZeros, &indices);

	check_flip_indices<V4F, mode6Partitions, mode6IndexResolution>(endpoints, &indices, 0, bc7DummyPartitionTableAllZeros);
	write_bc7_block(block, 6, 0, endpoints, indices);
	return error;
}

F32 compress_bc7_block_mode7(V4F pixels[16], BC7Block& block, V3F mins[numPartitionsFor2Subsets], V3F maxes[numPartitionsFor2Subsets], F32 minAlphas[numPartitionsFor2Subsets], F32 maxAlphas[numPartitionsFor2Subsets]) {
	if (!mode7Enable) {
		return F32_LARGE;
	}
	constexpr U32 mode7Partitions = 2;
	constexpr U32 mode7ComponentBits = 5;
	constexpr U32 mode7PBitsPerParition = 2;
	constexpr U32 mode7IndexResolution = 2;

	// Check each possible partition and find the one with lowest error
	F32 bestError = F32_LARGE;
	U32 bestPartition;
	V4F bestEndpoints[mode7Partitions * 2];
	U64 bestIndices;
	// It might be much better for speed to try to find the best pattern or several patterns first with a basic distance from points to line rather than checking them all.
	for (U32 partitionTable = 0; partitionTable < numPartitionTablesPerSubset; partitionTable++) {
		const BC7PartitionTable& table = bc7PartitionTable2Subsets[partitionTable];
		//Original endpoints can be min and max
		V4F endpoints[mode7Partitions * 2]{
			v4f(mins[partitionTable * mode7Partitions + 0], minAlphas[partitionTable * mode7Partitions + 0]),
			v4f(maxes[partitionTable * mode7Partitions + 0], maxAlphas[partitionTable * mode7Partitions + 0]),
			v4f(mins[partitionTable * mode7Partitions + 1], minAlphas[partitionTable * mode7Partitions + 1]),
			v4f(maxes[partitionTable * mode7Partitions + 1], maxAlphas[partitionTable * mode7Partitions + 1])
		};
		// I didn't think it was at first, but this is actually required for the degenerate case where the points all lie on opposing diagonals, projecting to the same point on the original diagonal (which results in least squares erroring).
		choose_best_diagonals_RGBA8<mode7Partitions>(pixels, endpoints, table);
		// Mode 1 has 2 partitions
		for (U32 partition = 0; partition < mode7Partitions; partition++) {
			// Find indices from the original endpoints and optimize with a least squares iteration to get the new endpoints
			least_squares_optimize_endpoints_RGBA8<mode7IndexResolution>(pixels, &endpoints[partition * 2], table, partition);
		}

		U64 indices;
		F32 error = quantize_bc7_endpoints<mode7Partitions, mode7ComponentBits, mode7PBitsPerParition, mode7IndexResolution>(pixels, endpoints, table, &indices);

		// Test this partition table's error, see if it's the best one yet
		if (error < bestError) {
			check_flip_indices<V4F, mode7Partitions, mode7IndexResolution>(endpoints, &indices, partitionTable, table);
			memcpy(bestEndpoints, endpoints, mode7Partitions * 2 * sizeof(V4F));
			bestIndices = indices;
			bestPartition = partitionTable;
			bestError = error;
		}
	}
	write_bc7_block(block, 7, bestPartition, bestEndpoints, bestIndices);
	return bestError;
}

void calc_min_max_mean(const BC7PartitionTable& partitionTable, V4F fPixels[16], V3F minV[3], V3F maxV[3], V3F mean[3]) {
	for (U32 i = 0; i < 3; i++) {
		minV[i] = { F32_LARGE, F32_LARGE, F32_LARGE };
		maxV[i] = { -F32_LARGE, -F32_LARGE , -F32_LARGE };
		mean[i] = { 0.0F, 0.0F, 0.0F };
	}

	F32 meanCounts[3]{ 0.0F, 0.0F, 0.0F };
	for (U32 i = 0; i < 16; i++) {
		U32 partition = partitionTable.partitionNumbers[i];
		minV[partition] = min(minV[partition], v3f_xyz(fPixels[i]));
		maxV[partition] = max(maxV[partition], v3f_xyz(fPixels[i]));
		mean[partition] += v3f_xyz(fPixels[i]);
		meanCounts[partition] += 1.0F;
	}

	for (U32 i = 0; i < 3; i++) {
		mean[i] /= meanCounts[i];
	}
}

void calc_min_max_f(const BC7PartitionTable& partitionTable, V4F fPixels[16], F32 mins[2], F32 maxes[2]) {
	for (U32 i = 0; i < 2; i++) {
		mins[i] = F32_LARGE;
		maxes[i] = -F32_LARGE;
	}
	for (U32 i = 0; i < 16; i++) {
		U32 partition = partitionTable.partitionNumbers[i];
		mins[partition] = min(mins[partition], fPixels[i].w);
		maxes[partition] = max(maxes[partition], fPixels[i].w);
	}
}

void calc_min_max_fx8(const BC7PartitionTable& partitionTable, V4Fx8 pixels[16], F32x8 mins[2], F32x8 maxes[2]) {
	for (U32 i = 0; i < 2; i++) {
		mins[i] = _mm256_set1_ps(F32_LARGE);
		maxes[i] = _mm256_set1_ps(-F32_LARGE);
	}
	for (U32 i = 0; i < 16; i++) {
		U32 partition = partitionTable.partitionNumbers[i];
		mins[partition] = _mm256_min_ps(mins[partition], pixels[i].w);
		maxes[partition] = _mm256_max_ps(maxes[partition], pixels[i].w);
	}
}

void calc_min_maxx8(const BC7PartitionTable& partitionTable, V4Fx8 fPixels[16], V3Fx8 mins[3], V3Fx8 maxs[3]) {
	F32x8 large = _mm256_set1_ps(F32_LARGE);
	F32x8 largeNegative = _mm256_set1_ps(-F32_LARGE);
	for (U32 i = 0; i < 3; i++) {
		mins[i] = V3Fx8{ large, large, large };
		maxs[i] = V3Fx8{ largeNegative, largeNegative, largeNegative };
	}
	for (U32 i = 0; i < 16; i++) {
		U32 partition = partitionTable.partitionNumbers[i];
		V3Fx8 rgb = v3fx8_xyz(fPixels[i]);
		mins[partition] = min(mins[partition], rgb);
		maxs[partition] = max(maxs[partition], rgb);
	}
}

void blend_bc7blockx8(BC7Blockx4 dst[2], BC7Blockx4 src[2], F32x8 mask) {
	// Extract the low and high halves of the mask and extend them to 64 bits
	F32x8 maskLow = _mm256_castsi256_ps(_mm256_cvtepi32_epi64(_mm256_castsi256_si128(_mm256_castps_si256(mask))));
	F32x8 maskHigh = _mm256_castsi256_ps(_mm256_cvtepi32_epi64(_mm256_extracti128_si256(_mm256_castps_si256(mask), 1)));
	// dst[0] = blend(dst[0], src[0], maskLow)
	dst[0].data[0] = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(dst[0].data[0]), _mm256_castsi256_ps(src[0].data[0]), maskLow));
	dst[0].data[1] = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(dst[0].data[1]), _mm256_castsi256_ps(src[0].data[1]), maskLow));
	// dst[1] = blend(dst[1], src[1], maskHigh)
	dst[1].data[0] = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(dst[1].data[0]), _mm256_castsi256_ps(src[1].data[0]), maskHigh));
	dst[1].data[1] = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(dst[1].data[1]), _mm256_castsi256_ps(src[1].data[1]), maskHigh));
}

F32 compress_bc7_blockx8(V4Fx8 pixels[16], BC7Blockx4 blocks[2]) {
	V3Fx8 mins[totalNumPartitions];
	V3Fx8 maxes[totalNumPartitions];
	F32x8 alphaMins[numPartitionsFor2Subsets];
	F32x8 alphaMaxes[numPartitionsFor2Subsets];
	for (U32 i = 0; i < numPartitionTablesPerSubset; i++) {
		calc_min_maxx8(bc7PartitionTable2Subsets[i], pixels, &mins[i * 2], &maxes[i * 2]);
	}
	// Calculate min/max for 3 subsets
	for (U32 i = 0; i < numPartitionTablesPerSubset; i++) {
		calc_min_maxx8(bc7PartitionTable3Subsets[i], pixels, &mins[numPartitionsFor2Subsets + i * 3], &maxes[numPartitionsFor2Subsets + i * 3]);
	}
	for (U32 i = 0; i < numPartitionTablesPerSubset; i++) {
		calc_min_max_fx8(bc7PartitionTable2Subsets[i], pixels, &alphaMins[i * 2], &alphaMaxes[i * 2]);
	}

	BC7Blockx4 testBlocks[2];
	F32x8 bestError = compress_bc7_block_mode01237x8<0, V3Fx8>(pixels, blocks, mins + numPartitionsFor2Subsets, maxes + numPartitionsFor2Subsets, alphaMins, alphaMaxes);

	F32x8 error = compress_bc7_block_mode01237x8<1, V3Fx8>(pixels, testBlocks, mins, maxes, alphaMins, alphaMaxes);
	blend_bc7blockx8(blocks, testBlocks, _mm256_cmp_ps(error, bestError, _CMP_LT_OQ));
	bestError = _mm256_min_ps(error, bestError);

	error = compress_bc7_block_mode01237x8<2, V3Fx8>(pixels, testBlocks, mins + numPartitionsFor2Subsets, maxes + numPartitionsFor2Subsets, alphaMins, alphaMaxes);
	blend_bc7blockx8(blocks, testBlocks, _mm256_cmp_ps(error, bestError, _CMP_LT_OQ));
	bestError = _mm256_min_ps(error, bestError);

	error = compress_bc7_block_mode01237x8<3, V3Fx8>(pixels, testBlocks, mins, maxes, alphaMins, alphaMaxes);
	blend_bc7blockx8(blocks, testBlocks, _mm256_cmp_ps(error, bestError, _CMP_LT_OQ));
	bestError = _mm256_min_ps(error, bestError);

	error = compress_bc7_block_mode4x8(pixels, testBlocks, mins, maxes, alphaMins, alphaMaxes);
	blend_bc7blockx8(blocks, testBlocks, _mm256_cmp_ps(error, bestError, _CMP_LT_OQ));
	bestError = _mm256_min_ps(error, bestError);

	error = compress_bc7_block_mode5x8(pixels, testBlocks, mins, maxes, alphaMins, alphaMaxes);
	blend_bc7blockx8(blocks, testBlocks, _mm256_cmp_ps(error, bestError, _CMP_LT_OQ));
	bestError = _mm256_min_ps(error, bestError);

	error = compress_bc7_block_mode6x8(pixels, testBlocks, mins, maxes, alphaMins, alphaMaxes);
	blend_bc7blockx8(blocks, testBlocks, _mm256_cmp_ps(error, bestError, _CMP_LT_OQ));
	bestError = _mm256_min_ps(error, bestError);

	error = compress_bc7_block_mode01237x8<7, V4Fx8>(pixels, testBlocks, mins, maxes, alphaMins, alphaMaxes);
	blend_bc7blockx8(blocks, testBlocks, _mm256_cmp_ps(error, bestError, _CMP_LT_OQ));
	bestError = _mm256_min_ps(error, bestError);

	return horizontal_sum(bestError);
}

F32 compress_bc7_block(RGBA8 pixels[16], BC7Block& block) {
	V4F fPixels[16];
	for (U32 i = 0; i < 16; i++) {
		fPixels[i] = V4F{ static_cast<F32>(pixels[i].r) / 255.0F, static_cast<F32>(pixels[i].g) / 255.0F, static_cast<F32>(pixels[i].b) / 255.0F, static_cast<F32>(pixels[i].a) / 255.0F };
	}

	// We're going to precalculate all the min/max/mean values for each subset so we don't have to do recomputation for each mode
	V3F min[totalNumPartitions];
	V3F max[totalNumPartitions];
	V3F mean[totalNumPartitions];

	F32 alphaMins[numPartitionsFor2Subsets];
	F32 alphaMaxes[numPartitionsFor2Subsets];

	// Calculate min/max/mean for 2 subsets
	for (U32 i = 0; i < numPartitionTablesPerSubset; i++) {
		calc_min_max_mean(bc7PartitionTable2Subsets[i], fPixels, &min[i * 2], &max[i * 2], &mean[i * 2]);
	}
	// Calculate min/max/mean for 3 subsets
	for (U32 i = 0; i < numPartitionTablesPerSubset; i++) {
		calc_min_max_mean(bc7PartitionTable3Subsets[i], fPixels, &min[numPartitionsFor2Subsets + i * 3], &max[numPartitionsFor2Subsets + i * 3], &mean[numPartitionsFor2Subsets + i * 3]);
	}
	for (U32 i = 0; i < numPartitionTablesPerSubset; i++) {
		calc_min_max_f(bc7PartitionTable2Subsets[i], fPixels, &alphaMins[i * 2], &alphaMaxes[i * 2]);
	}

	BC7Block testBlock;
	F32 bestError;
	F32 error = compress_bc7_block_mode0(fPixels, testBlock, min + numPartitionsFor2Subsets, max + numPartitionsFor2Subsets, mean + numPartitionsFor2Subsets);
	bestError = error;
	block = testBlock;
	error = compress_bc7_block_mode1(fPixels, testBlock, min, max, mean);
	if (error < bestError) {
		bestError = error;
		block = testBlock;
	}
	error = compress_bc7_block_mode2(fPixels, testBlock, min + numPartitionsFor2Subsets, max + numPartitionsFor2Subsets, mean + numPartitionsFor2Subsets);
	if (error < bestError) {
		bestError = error;
		block = testBlock;
	}
	error = compress_bc7_block_mode3(fPixels, testBlock, min, max, mean);
	if (error < bestError) {
		bestError = error;
		block = testBlock;
	}
	error = compress_bc7_block_mode4(fPixels, testBlock, min, max, alphaMins, alphaMaxes);
	if (error < bestError) {
		bestError = error;
		block = testBlock;
	}
	error = compress_bc7_block_mode5(fPixels, testBlock, min, max, alphaMins, alphaMaxes);
	if (error < bestError) {
		bestError = error;
		block = testBlock;
	}
	error = compress_bc7_block_mode6(fPixels, testBlock, min, max, alphaMins, alphaMaxes);
	if (error < bestError) {
		bestError = error;
		block = testBlock;
	}
	error = compress_bc7_block_mode7(fPixels, testBlock, min, max, alphaMins, alphaMaxes);
	if (error < bestError) {
		bestError = error;
		block = testBlock;
	}
	return bestError;
}

#define BC7_ENABLE_AVX2 1

struct BC7AVXBlockRange {
	U32 begin;
	U32 end;
	F32 finalError;
	V4Fx8* pixelBlocks;
	BC7Block* blocks;
	U32* blockIndices;
	U32 opaqueIndex;
};

DWORD __stdcall bc7_avx_compress_block_range(LPVOID param) {
	BC7AVXBlockRange* range = (BC7AVXBlockRange*)param;
	U32 begin = range->begin;
	U32 end = range->end;
	BC7Block* blocks = range->blocks;
	U32* blockIndices = range->blockIndices;
	V4Fx8* pixelBlocks = range->pixelBlocks;
	U32 opaqueIndex = range->opaqueIndex;
	F32 finalError = 0.0F;
	for (U32 i = begin; i < end; i++) {
		BC7Blockx4 finalBlocks[2];
		alignas(32) U64 data0[8];
		alignas(32) U64 data1[8];
		finalError += compress_bc7_blockx8(&pixelBlocks[i * 16], finalBlocks);
		_mm256_store_si256(reinterpret_cast<U64x4*>(data0), finalBlocks[0].data[0]);
		_mm256_store_si256(reinterpret_cast<U64x4*>(data0 + 4), finalBlocks[1].data[0]);
		_mm256_store_si256(reinterpret_cast<U64x4*>(data1), finalBlocks[0].data[1]);
		_mm256_store_si256(reinterpret_cast<U64x4*>(data1 + 4), finalBlocks[1].data[1]);
		for (U32 j = 0; j < min(opaqueIndex - i * 8, 8ui32); j++) {
			blocks[blockIndices[i * 8 + j]] = BC7Block{ data0[j], data1[j] };
		}
	}
	range->finalError += finalError;
	return 0;
}

// Compresion //
BC7Block* compress_bc7(MemoryArena& resultArena, RGBA8* image, U32 width, U32 height, U32 workerThreadCount, F32* error) {
	if (!image) {
		return nullptr;
	}
	U32 blockWidth = (width + 3) / 4;
	U32 blockHeight = (height + 3) / 4;
	U32 numBlocks = blockWidth * blockHeight;
	BC7Block* blocks = resultArena.alloc<BC7Block>(numBlocks);

	F32 finalError = 0;

#if BC7_ENABLE_AVX2 == 0
	// 4x4 block
	RGBA8 pixels[4 * 4];
	for (U32 y = 0; y < blockHeight; y++) {
		for (U32 x = 0; x < blockWidth; x++) {
			BCCommon::fill_pixel_block(image, pixels, x, y, width, height);
			BC7Block& block = blocks[y * blockWidth + x];
			finalError += compress_bc7_block(pixels, block);
		}
	}
#else
	MemoryArena& scratchArena = get_scratch_arena_excluding(resultArena);
	MEMORY_ARENA_FRAME(scratchArena) {
		U32 simdBlockCount = (numBlocks + 7) / 8 + 1;
		// YMM align to 32
		U64 alignment = 32;
		V4Fx8* pixelBlocks = scratchArena.alloc_aligned<V4Fx8>(16 * simdBlockCount, alignment);
		// Add 7 extra blocks so we can read over the edge safely
		U32* blockIndices = scratchArena.alloc<U32>(numBlocks + 7);

		U32 opaqueIndex = 0;
		U32 transparentIndex = simdBlockCount - 1;
		for (U32 y = 0; y < blockHeight; y++) {
			for (U32 x = 0; x < blockWidth; x++) {
				BCCommon::fill_pixel_blockx8(image, (F32*)pixelBlocks, opaqueIndex, x, y, width, height);
				blockIndices[opaqueIndex] = y * blockWidth + x;
				opaqueIndex++;
			}
		}

		HANDLE* threads = scratchArena.alloc<HANDLE>(workerThreadCount);
		BC7AVXBlockRange* ranges = scratchArena.alloc<BC7AVXBlockRange>(workerThreadCount);
		U32 numOpaqueBlocksx8 = (opaqueIndex + 7) / 8;
		U32 step = numOpaqueBlocksx8 / workerThreadCount;
		U32 leftover = numOpaqueBlocksx8 % workerThreadCount;
		U32 startIdx = 0;
		for (U32 i = 0; i < workerThreadCount; i++) {
			ranges[i].begin = startIdx;
			ranges[i].end = startIdx + step + (i < leftover);
			startIdx += step + (i < leftover);
			ranges[i].pixelBlocks = pixelBlocks;
			ranges[i].blocks = blocks;
			ranges[i].blockIndices = blockIndices;
			ranges[i].opaqueIndex = opaqueIndex;
			threads[i] = CreateThread(NULL, 512 * KILOBYTE, bc7_avx_compress_block_range, &ranges[i], 0, NULL);
		}
		WaitForMultipleObjects(workerThreadCount, threads, TRUE, INFINITE);
		for (U32 i = 0; i < workerThreadCount; i++) {
			finalError += ranges[i].finalError;
			CloseHandle(threads[i]);
		}
	}
#endif
	// Slightly inaccurate error since it doesn't account for the padding pixels for non multiple of 4 textures and I don't feel like implementing that
	if (error) {
		*error = finalError / static_cast<F32>(numBlocks * 16 * 4);
	}
	return blocks;
}

// Decompression //

U32 extract_mode(BC7Block& block) {
	return U32(tzcnt64(block.data[0]));
}

void decompress_bc7_mode0(BC7Block& block, RGBA8 pixels[16]) {
	// 4 bits of partition selection
	U32 partitionSelection = (block.data[0] >> 1) & 0b1111;
	// 3 partitions
	const BC7PartitionTable& partition = bc7PartitionTable3Subsets[partitionSelection];
	U8 anchor2nd = bc7PartitionTable3Anchors2ndSubset[partitionSelection];
	U8 anchor3rd = bc7PartitionTable3Anchors3rdSubset[partitionSelection];

	U32 endpointReds = block.data[0] >> 5;
	U32 endpointGreens = block.data[0] >> 29;
	U32 endpointBlues = (block.data[0] >> 53) | (block.data[1] << (64 - 53));
	U32 endpointPBits = block.data[1] >> 13;
	// Mode 0 has 3 endpoints
	RGB8 endpoints[6];
	for (U32 i = 0; i < 6; i++) {
		U32 pBit = ((endpointPBits >> i) & 1) << 3;
		// 4 bit colors
		endpoints[i].r = (endpointReds >> (i * 4)) & 0b1111;
		endpoints[i].r = (endpoints[i].r << 4) | pBit | (endpoints[i].r >> 1);
		endpoints[i].g = (endpointGreens >> (i * 4)) & 0b1111;
		endpoints[i].g = (endpoints[i].g << 4) | pBit | (endpoints[i].g >> 1);
		endpoints[i].b = (endpointBlues >> (i * 4)) & 0b1111;
		endpoints[i].b = (endpoints[i].b << 4) | pBit | (endpoints[i].b >> 1);
	}

	U64 indices = block.data[1] >> 19;

	// Decode pixels
	U32 shift = 0;
	for (U32 i = 0; i < 16; i++) {
		// 3 bit indices
		U32 index;
		if (anchor2nd == i || anchor3rd == i || 0 == i) {
			index = (indices >> shift) & 0b11;
			shift += 2;
		} else {
			index = (indices >> shift) & 0b111;
			shift += 3;
		}

		U32 interpolationFactor = bc7InterpolationFactors3[index];
		U8 partitionNumber = partition.partitionNumbers[i];
		RGB8 e0 = endpoints[partitionNumber * 2];
		RGB8 e1 = endpoints[partitionNumber * 2 + 1];
		pixels[i].r = bc7_interpolate(e0.r, e1.r, interpolationFactor);
		pixels[i].g = bc7_interpolate(e0.g, e1.g, interpolationFactor);
		pixels[i].b = bc7_interpolate(e0.b, e1.b, interpolationFactor);
		pixels[i].a = 255;
	}
}

void decompress_bc7_mode1(BC7Block& block, RGBA8 pixels[16]) {
	// 6 bits of partition selection
	U32 partitionSelection = (block.data[0] >> 2) & 0b111111;
	// 2 partitions
	const BC7PartitionTable& partition = bc7PartitionTable2Subsets[partitionSelection];
	U8 anchor2nd = bc7PartitionTable2Anchors2ndSubset[partitionSelection];

	U32 endpointReds = block.data[0] >> 8;
	U32 endpointGreens = block.data[0] >> 32;
	U32 endpointBlues = (block.data[0] >> 56) | (block.data[1] << (64 - 56));
	U32 endpointPBits = block.data[1] >> 16;
	// Mode 1 has 2 endpoints
	RGB8 endpoints[4];
	for (U32 i = 0; i < 4; i++) {
		U32 pBit = ((endpointPBits >> (i >> 1)) & 1) << 1;
		// 6 bit colors
		endpoints[i].r = (endpointReds >> (i * 6)) & 0b111111;
		endpoints[i].r = (endpoints[i].r << 2) | pBit | (endpoints[i].r >> 5);
		endpoints[i].g = (endpointGreens >> (i * 6)) & 0b111111;
		endpoints[i].g = (endpoints[i].g << 2) | pBit | (endpoints[i].g >> 5);
		endpoints[i].b = (endpointBlues >> (i * 6)) & 0b111111;
		endpoints[i].b = (endpoints[i].b << 2) | pBit | (endpoints[i].b >> 5);
	}

	U64 indices = block.data[1] >> 18;

	// Decode pixels
	U32 shift = 0;
	for (U32 i = 0; i < 16; i++) {
		// 3 bit indices
		U32 index;
		if (anchor2nd == i || 0 == i) {
			index = (indices >> shift) & 0b11;
			shift += 2;
		} else {
			index = (indices >> shift) & 0b111;
			shift += 3;
		}

		U32 interpolationFactor = bc7InterpolationFactors3[index];
		U8 partitionNumber = partition.partitionNumbers[i];
		RGB8 e0 = endpoints[partitionNumber * 2];
		RGB8 e1 = endpoints[partitionNumber * 2 + 1];
		pixels[i].r = bc7_interpolate(e0.r, e1.r, interpolationFactor);
		pixels[i].g = bc7_interpolate(e0.g, e1.g, interpolationFactor);
		pixels[i].b = bc7_interpolate(e0.b, e1.b, interpolationFactor);
		pixels[i].a = 255;
	}
}

void decompress_bc7_mode2(BC7Block& block, RGBA8 pixels[16]) {
	// 6 bits of partition selection
	U32 partitionSelection = (block.data[0] >> 3) & 0b111111;
	// 3 partitions
	const BC7PartitionTable& partition = bc7PartitionTable3Subsets[partitionSelection];
	U8 anchor2nd = bc7PartitionTable3Anchors2ndSubset[partitionSelection];
	U8 anchor3rd = bc7PartitionTable3Anchors3rdSubset[partitionSelection];

	U32 endpointReds = block.data[0] >> 9;
	U32 endpointGreens = (block.data[0] >> 39) | (block.data[1] << (64 - 39));
	U32 endpointBlues = block.data[1] >> 5;
	// Mode 2 has 3 endpoints
	RGB8 endpoints[6];
	for (U32 i = 0; i < 6; i++) {
		// 5 bit colors
		endpoints[i].r = (endpointReds >> (i * 5)) & 0b11111;
		endpoints[i].r = (endpoints[i].r << 3) | (endpoints[i].r >> 2);
		endpoints[i].g = (endpointGreens >> (i * 5)) & 0b11111;
		endpoints[i].g = (endpoints[i].g << 3) | (endpoints[i].g >> 2);
		endpoints[i].b = (endpointBlues >> (i * 5)) & 0b11111;
		endpoints[i].b = (endpoints[i].b << 3) | (endpoints[i].b >> 2);
	}

	U64 indices = block.data[1] >> 35;

	// Decode pixels
	U32 shift = 0;
	for (U32 i = 0; i < 16; i++) {
		// 2 bit indices
		U32 index;
		if (anchor2nd == i || anchor3rd == i || 0 == i) {
			index = (indices >> shift) & 0b1;
			shift += 1;
		} else {
			index = (indices >> shift) & 0b11;
			shift += 2;
		}

		U32 interpolationFactor = bc7InterpolationFactors2[index];
		U8 partitionNumber = partition.partitionNumbers[i];
		RGB8 e0 = endpoints[partitionNumber * 2];
		RGB8 e1 = endpoints[partitionNumber * 2 + 1];
		pixels[i].r = bc7_interpolate(e0.r, e1.r, interpolationFactor);
		pixels[i].g = bc7_interpolate(e0.g, e1.g, interpolationFactor);
		pixels[i].b = bc7_interpolate(e0.b, e1.b, interpolationFactor);
		pixels[i].a = 255;
	}
}

void decompress_bc7_mode3(BC7Block& block, RGBA8 pixels[16]) {
	// 6 bits of partition selection
	U32 partitionSelection = (block.data[0] >> 4) & 0b111111;
	// 2 partitions
	const BC7PartitionTable& partition = bc7PartitionTable2Subsets[partitionSelection];
	U8 anchor2nd = bc7PartitionTable2Anchors2ndSubset[partitionSelection];

	U32 endpointReds = block.data[0] >> 10;
	U32 endpointGreens = (block.data[0] >> 38) | (block.data[1] << (64 - 38));
	U32 endpointBlues = block.data[1] >> 2;
	U32 endpointPBits = block.data[1] >> 30;
	// Mode 3 has 2 endpoints
	RGB8 endpoints[4];
	for (U32 i = 0; i < 4; i++) {
		U32 pBit = (endpointPBits >> i) & 1;
		// 7 bit colors
		endpoints[i].r = (endpointReds >> (i * 7)) & 0b1111111;
		endpoints[i].r = (endpoints[i].r << 1) | pBit;
		endpoints[i].g = (endpointGreens >> (i * 7)) & 0b1111111;
		endpoints[i].g = (endpoints[i].g << 1) | pBit;
		endpoints[i].b = (endpointBlues >> (i * 7)) & 0b1111111;
		endpoints[i].b = (endpoints[i].b << 1) | pBit;
	}

	U64 indices = block.data[1] >> 34;

	// Decode pixels
	U32 shift = 0;
	for (U32 i = 0; i < 16; i++) {
		// 2 bit indices
		U32 index;
		if (anchor2nd == i || 0 == i) {
			index = (indices >> shift) & 0b1;
			shift += 1;
		} else {
			index = (indices >> shift) & 0b11;
			shift += 2;
		}

		U32 interpolationFactor = bc7InterpolationFactors2[index];
		U8 partitionNumber = partition.partitionNumbers[i];
		RGB8 e0 = endpoints[partitionNumber * 2];
		RGB8 e1 = endpoints[partitionNumber * 2 + 1];
		pixels[i].r = bc7_interpolate(e0.r, e1.r, interpolationFactor);
		pixels[i].g = bc7_interpolate(e0.g, e1.g, interpolationFactor);
		pixels[i].b = bc7_interpolate(e0.b, e1.b, interpolationFactor);
		pixels[i].a = 255;
	}
}

RGBA8 rotate_channels(RGBA8 pixel, U32 rotationBits){
	switch (rotationBits) {
	case 0b00: return pixel;
	case 0b01: return RGBA8{ pixel.a, pixel.g, pixel.b, pixel.r };
	case 0b10: return RGBA8{ pixel.r, pixel.a, pixel.b, pixel.g };
	case 0b11: return RGBA8{ pixel.r, pixel.g, pixel.a, pixel.b };
	default: RUNTIME_ASSERT(false, "Bad rotation"a); break;
	}
}

void decompress_bc7_mode4(BC7Block& block, RGBA8 pixels[16]) {
	U32 rotationBits = (block.data[0] >> 5) & 0b11;
	U32 indexSelection = (block.data[0] >> 7) & 1;

	RGBA8 endpoints[2];
	for (U32 i = 0; i < 2; i++) {
		// 5 bit color
		U32 r = (block.data[0] >> (8 + i * 5)) & 0b11111;
		U32 g = (block.data[0] >> (18 + i * 5)) & 0b11111;
		U32 b = (block.data[0] >> (28 + i * 5)) & 0b11111;
		// 6 bit alpha
		U32 a = (block.data[0] >> (38 + i * 6)) & 0b111111;

		endpoints[i].r = (r << 3) | (r >> 2);
		endpoints[i].g = (g << 3) | (g >> 2);
		endpoints[i].b = (b << 3) | (b >> 2);
		endpoints[i].a = (a << 2) | (a >> 4);
	}

	U64 indicesPrimary = (block.data[0] >> 50) | (block.data[1] << (64 - 50));
	U64 indicesSecondary = block.data[1] >> 17;
	U32 shiftPrimary = 0;
	U32 shiftSecondary = 0;
	for (U32 i = 0; i < 16; i++) {
		// 2 and 3 bit indices
		U32 indexPrimary;
		U32 indexSecondary;
		if (0 == i) {
			indexPrimary = (indicesPrimary >> shiftPrimary) & 0b1;
			indexSecondary = (indicesSecondary >> shiftSecondary) & 0b11;
			shiftPrimary += 1;
			shiftSecondary += 2;
		} else {
			indexPrimary = (indicesPrimary >> shiftPrimary) & 0b11;
			indexSecondary = (indicesSecondary >> shiftSecondary) & 0b111;
			shiftPrimary += 2;
			shiftSecondary += 3;
		}

		U32 interpolationFactorPrimary = bc7InterpolationFactors2[indexPrimary];
		U32 interpolationFactorSecondary = bc7InterpolationFactors3[indexSecondary];
		RGBA8 e0 = endpoints[0];
		RGBA8 e1 = endpoints[1];
		if (indexSelection) {
			// Use secondary for color
			pixels[i].r = bc7_interpolate(e0.r, e1.r, interpolationFactorSecondary);
			pixels[i].g = bc7_interpolate(e0.g, e1.g, interpolationFactorSecondary);
			pixels[i].b = bc7_interpolate(e0.b, e1.b, interpolationFactorSecondary);
			pixels[i].a = bc7_interpolate(e0.a, e1.a, interpolationFactorPrimary);
		} else {
			// Use primary for color
			pixels[i].r = bc7_interpolate(e0.r, e1.r, interpolationFactorPrimary);
			pixels[i].g = bc7_interpolate(e0.g, e1.g, interpolationFactorPrimary);
			pixels[i].b = bc7_interpolate(e0.b, e1.b, interpolationFactorPrimary);
			pixels[i].a = bc7_interpolate(e0.a, e1.a, interpolationFactorSecondary);
		}
		pixels[i] = rotate_channels(pixels[i], rotationBits);
	}
}

void decompress_bc7_mode5(BC7Block& block, RGBA8 pixels[16]) {
	U32 rotationBits = (block.data[0] >> 6) & 0b11;
	U32 indexSelection = (block.data[0] >> 7) & 1;

	RGBA8 endpoints[2];
	// 8 bit alpha, can extract directly
	endpoints[0].a = (block.data[0] >> 50) & 0xFF;
	endpoints[1].a = ((block.data[0] >> 58) | (block.data[1] << (64 - 58))) & 0xFF;
	for (U32 i = 0; i < 2; i++) {
		// 7 bit color
		U32 r = (block.data[0] >> (8 + i * 7)) & 0b1111111;
		U32 g = (block.data[0] >> (22 + i * 7)) & 0b1111111;
		// Ah man, spotted an error in the specification here (listed 35 instead of 36). Turns out I was using an outdated spec version and it was fixed later. I hope I didn't embed any other old spec errors in my code.
		U32 b = (block.data[0] >> (36 + i * 7)) & 0b1111111;

		endpoints[i].r = (r << 1) | (r >> 6);	
		endpoints[i].g = (g << 1) | (g >> 6);
		endpoints[i].b = (b << 1) | (b >> 6);
	}

	U64 indicesPrimary = block.data[1] >> 2;
	U64 indicesSecondary = block.data[1] >> 33;
	U32 shift = 0;
	for (U32 i = 0; i < 16; i++) {
		// 2 bit indices
		U32 indexPrimary;
		U32 indexSecondary;
		if (0 == i) {
			indexPrimary = (indicesPrimary >> shift) & 0b1;
			indexSecondary = (indicesSecondary >> shift) & 0b1;
			shift += 1;
		} else {
			indexPrimary = (indicesPrimary >> shift) & 0b11;
			indexSecondary = (indicesSecondary >> shift) & 0b11;
			shift += 2;
		}

		U32 interpolationFactorPrimary = bc7InterpolationFactors2[indexPrimary];
		U32 interpolationFactorSecondary = bc7InterpolationFactors2[indexSecondary];
		RGBA8 e0 = endpoints[0];
		RGBA8 e1 = endpoints[1];
		pixels[i].r = bc7_interpolate(e0.r, e1.r, interpolationFactorPrimary);
		pixels[i].g = bc7_interpolate(e0.g, e1.g, interpolationFactorPrimary);
		pixels[i].b = bc7_interpolate(e0.b, e1.b, interpolationFactorPrimary);
		pixels[i].a = bc7_interpolate(e0.a, e1.a, interpolationFactorSecondary);
		pixels[i] = rotate_channels(pixels[i], rotationBits);
	}
}

void decompress_bc7_mode6(BC7Block& block, RGBA8 pixels[16]) {
	U32 pBits = (block.data[0] >> 63) | (block.data[1] << 1);
	// Mode 6 has 1 endpoint
	RGBA8 endpoints[2];
	for (U32 i = 0; i < 2; i++) {
		U32 pBit = (pBits >> i) & 1;
		// 7 bit color
		U32 r = (block.data[0] >> (7 + i * 7)) & 0b1111111;
		U32 g = (block.data[0] >> (21 + i * 7)) & 0b1111111;
		U32 b = (block.data[0] >> (35 + i * 7)) & 0b1111111;
		U32 a = (block.data[0] >> (49 + i * 7)) & 0b1111111;

		endpoints[i].r = (r << 1) | pBit;
		endpoints[i].g = (g << 1) | pBit;
		endpoints[i].b = (b << 1) | pBit;
		endpoints[i].a = (a << 1) | pBit;
	}

	U64 indices = block.data[1] >> 1;
	U32 shift = 0;
	for (U32 i = 0; i < 16; i++) {
		// 4 bit indices
		U32 index;
		if (0 == i) {
			index = (indices >> shift) & 0b111;
			shift += 3;
		} else {
			index = (indices >> shift) & 0b1111;
			shift += 4;
		}

		U32 interpolationFactor = bc7InterpolationFactors4[index];
		RGBA8 e0 = endpoints[0];
		RGBA8 e1 = endpoints[1];
		pixels[i].r = bc7_interpolate(e0.r, e1.r, interpolationFactor);
		pixels[i].g = bc7_interpolate(e0.g, e1.g, interpolationFactor);
		pixels[i].b = bc7_interpolate(e0.b, e1.b, interpolationFactor);
		pixels[i].a = bc7_interpolate(e0.a, e1.a, interpolationFactor);
	}
}

void decompress_bc7_mode7(BC7Block& block, RGBA8 pixels[16]) {
	// 6 bits of partition selection
	U32 partitionSelection = (block.data[0] >> 8) & 0b111111;
	// 2 partitions
	const BC7PartitionTable& partition = bc7PartitionTable2Subsets[partitionSelection];
	U8 anchor2nd = bc7PartitionTable2Anchors2ndSubset[partitionSelection];

	U32 endpointReds = block.data[0] >> 14;
	U32 endpointGreens = block.data[0] >> 34;
	U32 endpointBlues = (block.data[0] >> 54) | (block.data[1] << (64 - 54));
	U32 endpointAlphas = block.data[1] >> 10;
	U32 endpointPBits = block.data[1] >> 30;
	// Mode 7 has 2 endpoints
	RGBA8 endpoints[4];
	for (U32 i = 0; i < 4; i++) {
		U32 pBit = ((endpointPBits >> i) & 1) << 2;
		// 5 bit colors
		endpoints[i].r = (endpointReds >> (i * 5)) & 0b11111;
		endpoints[i].r = (endpoints[i].r << 3) | pBit | (endpoints[i].r >> 3);
		endpoints[i].g = (endpointGreens >> (i * 5)) & 0b11111;
		endpoints[i].g = (endpoints[i].g << 3) | pBit | (endpoints[i].g >> 3);
		endpoints[i].b = (endpointBlues >> (i * 5)) & 0b11111;
		endpoints[i].b = (endpoints[i].b << 3) | pBit | (endpoints[i].b >> 3);
		endpoints[i].a = (endpointAlphas >> (i * 5)) & 0b11111;
		endpoints[i].a = (endpoints[i].a << 3) | pBit | (endpoints[i].a >> 3);
	}

	U64 indices = block.data[1] >> 34;
	U32 shift = 0;
	for (U32 i = 0; i < 16; i++) {
		// 2 bit indices
		U32 index;
		if (anchor2nd == i || 0 == i) {
			index = (indices >> shift) & 0b1;
			shift += 1;
		} else {
			index = (indices >> shift) & 0b11;
			shift += 2;
		}

		U32 interpolationFactor = bc7InterpolationFactors2[index];
		U8 partitionNumber = partition.partitionNumbers[i];
		RGBA8 e0 = endpoints[partitionNumber * 2];
		RGBA8 e1 = endpoints[partitionNumber * 2 + 1];
		pixels[i].r = bc7_interpolate(e0.r, e1.r, interpolationFactor);
		pixels[i].g = bc7_interpolate(e0.g, e1.g, interpolationFactor);
		pixels[i].b = bc7_interpolate(e0.b, e1.b, interpolationFactor);
		pixels[i].a = bc7_interpolate(e0.a, e1.a, interpolationFactor);
	}
}

void decompress_bc7_block(BC7Block& block, RGBA8 pixels[16]) {
	U32 mode = extract_mode(block);
	// Since most of these are nearly the same I could have made one or two more generic methods to decode all of them.
	// I find it easier to think about if I only have to consider one mode at a time
	switch (mode) {
	case 0: decompress_bc7_mode0(block, pixels); break;
	case 1: decompress_bc7_mode1(block, pixels); break;
	case 2: decompress_bc7_mode2(block, pixels); break;
	case 3: decompress_bc7_mode3(block, pixels); break;
	case 4: decompress_bc7_mode4(block, pixels); break;
	case 5: decompress_bc7_mode5(block, pixels); break;
	case 6: decompress_bc7_mode6(block, pixels); break;
	case 7: decompress_bc7_mode7(block, pixels); break;
	default:
		// Error
		memset(pixels, 0, 16 * sizeof(RGBA8));
		return;
	}
}

RGBA8* decompress_bc7(MemoryArena& resultArena, BC7Block* blocks, U32 finalWidth, U32 finalHeight) {
	U32 blockWidth = (finalWidth + 3) / 4;
	U32 blockHeight = (finalHeight + 3) / 4;
	RGBA8* finalImage = resultArena.alloc<RGBA8>(finalWidth * finalHeight);
	// 4x4 block
	RGBA8 pixels[4 * 4];
	for (U32 y = 0; y < blockHeight; y++) {
		for (U32 x = 0; x < blockWidth; x++) {
			BC7Block& block = blocks[y * blockWidth + x];
			decompress_bc7_block(block, pixels);
			BCCommon::copy_block_pixels_to_image(pixels, finalImage, x, y, finalWidth, finalHeight);
		}
	}
	return finalImage;
}

}