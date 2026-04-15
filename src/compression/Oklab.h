#pragma once

#include "../DrillLib.h"

namespace Oklab {

// Credit to Bjorn Ottosson for Oklab
// https://bottosson.github.io/posts/oklab/

// All colors will be invalid in sRGB past this point
const F32 SRGB_PICKER_CHROMA_END = 0.37F;

V3F linear_to_oklab(V3F rgb) {
	F32 l = 0.4122214708F * rgb.x + 0.5363325363F * rgb.y + 0.0514459929F * rgb.z;
	F32 m = 0.2119034982F * rgb.x + 0.6806995451F * rgb.y + 0.1073969566F * rgb.z;
	F32 s = 0.0883024619F * rgb.x + 0.2817188376F * rgb.y + 0.6299787005F * rgb.z;
	l = cbrtf32(l);
	m = cbrtf32(m);
	s = cbrtf32(s);
	F32 L = 0.2104542553F * l + 0.7936177850F * m - 0.0040720468F * s;
	F32 a = 1.9779984951F * l - 2.4285922050F * m + 0.4505937099F * s;
	F32 b = 0.0259040371F * l + 0.7827717662F * m - 0.8086757660F * s;
	return V3F{ L, a, b };
}

V3F oklab_to_linear(V3F lab) {
	F32 l = lab.x + 0.3963377774F * lab.y + 0.2158037573F * lab.z;
	F32 m = lab.x - 0.1055613458F * lab.y - 0.0638541728F * lab.z;
	F32 s = lab.x - 0.0894841775F * lab.y - 1.2914855480F * lab.z;
	l = l * l * l;
	m = m * m * m;
	s = s * s * s;
	F32 r =  4.0767416621F * l - 3.3077115913F * m + 0.2309699292F * s;
	F32 g = -1.2684380046F * l + 2.6097574011F * m - 0.3413193965F * s;
	F32 b = -0.0041960863F * l - 0.7034186147F * m + 1.7076147010F * s;
	return V3F{ r, g, b };
}

V3F oklab_to_srgb(V3F lab) {
	V3F rgb = oklab_to_linear(lab);
	return to_srgb(rgb);
}

V3F srgb_to_oklab(V3F srgb) {
	V3F rgb = from_srgb(srgb);
	return linear_to_oklab(rgb);
}

F32 oklab_l_to_lightness(F32 L) {
	F32 k1 = 0.206F;
	F32 k2 = 0.03F;
	F32 k3 = (1.0F + k1) / (1.0F + k2);
	return 0.5F * (k3 * L - k1 + sqrtf32(sqr(k3 * L - k1) + 4.0F * k2 * k3 * L));
}

F32 lightness_to_oklab_l(F32 lightness) {
	F32 k1 = 0.206F;
	F32 k2 = 0.03F;
	F32 k3 = (1.0F + k1) / (1.0F + k2);
	return (lightness * (lightness + k1)) / (k3 * (lightness + k2));
}

V3F lab_to_lch(V3F lab) {
	return V3F{ lab.x, sqrtf32(lab.y * lab.y + lab.z * lab.z), atan2f32(lab.z, lab.y) };
}

V3F lch_to_lab(V3F lch) {
	F32 sinH;
	F32 cosH = sincosf32(&sinH, lch.z);
	return V3F{ lch.x, lch.y * cosH, lch.y * sinH };
}

V3F lab_to_lrch(V3F lab) {
	return V3F{ oklab_l_to_lightness(lab.x), sqrtf32(lab.y * lab.y + lab.z * lab.z), atan2f32(lab.z, lab.y)};
}

V3F lrch_to_lab(V3F lch) {
	F32 sinH;
	F32 cosH = sincosf32(&sinH, lch.z);
	return V3F{ lightness_to_oklab_l(lch.x), lch.y * cosH, lch.y * sinH};
}

V3F lrch_to_srgb(V3F lrch) {
	return oklab_to_srgb(lrch_to_lab(lrch));
};

V3F lrch_to_linear(V3F lrch) {
	return oklab_to_linear(lrch_to_lab(lrch));
};

// https://bottosson.github.io/posts/gamutclipping/

// Naturally, I was opposed to copying someone else's code, especially, when I didn't understand it, so I decided to figure out my own solution for the clipping described in the article
// My solution uses a direct solve with the cubic formula instead of a curve fit and Halley refinement.
// This function is slower and possibly less precise (I didn't bother to check, but it might be, given the large number of intermediate calculations and single precision) than the one in the post, but it is mine
V3F clip_lrch_to_srgb_gamut(V3F lrch) {
	if (lrch.y < 0.0F) {
		return V3F{ clamp01(lrch.x), 0.0F, clamp01(lrch.z) };
	}
	V3F lch{ lightness_to_oklab_l(lrch.x), lrch.y, lrch.z };

	// We want to find coefficients k for a cubic equation that we can set equal to 1 or 0
	// Calling the first conversion to RGB matrix u and the second (after the cube step) v
	// l = u00 * L + u01 * cos(h) * c + u02 * sin(h) * c, similar for m and s
	// u00/u10/u20 == 1,
	// l = L + C * (u01 * cos(h) + u02 * sin(h))
	// Hue is constant,
	// w0 = u01 * cos(h) + u02 * sin(h)
	// l = L + w0 * C, similar for m and s
	// v00 * l^3 + v01 * m^3 + v02 * s^3, set equal to 0 or 1
	// Substitute in the equations for l/m/s,
	// v00 * (L + w0 * C)^3 + v01 * (L + w1 * C)^3 + v02 * (L + w2 * C)^3
	// (L + w * C)^3 == L^3 + 3 * L^2 * w * C + 3 * L * w^2 * C^2 + w^3 * C^3
	// v00 * (L^3 + 3 * L^2 * w0 * C + 3 * L * w0^2 * C^2 + w0^3 * C^3) ... and on for the other two
	// (v00 + v01 + v02) * L^3 == L^3, the v rows sum to 1, no need for a first coefficient
	// 3 * (v00 * w0 + v01 * w1 + v02 * v2) * L^2 * C == 3 * dot(v0, w) * L^2 * C
	// k0 = 3 * dot(v0, w)
	// 3 * (v00 * w0^2 + v01 * w1^2 + v02 * w^2) * L * C^2 == 3 * dot(v, w^2) * L * C^2
	// k1 = 3 * dot(v, w^2)
	// (v00 * w0^3 + v01 * w1^3 + v02 * w2^3) * C^3 == dot(v, w^3) * C^3
	// k2 = dot(v, w^3)
	F32 sinH;
	F32 cosH = sincosf32(&sinH, lch.z);
	F32 w0 = cosH * 0.3963377774F + sinH * 0.2158037573F;
	F32 w1 = cosH * -0.1055613458F + sinH * -0.0638541728F;
	F32 w2 = cosH * -0.0894841775F + sinH * -1.2914855480F;
	V3F vr = V3F{ 4.0767416621F, -3.3077115913F, 0.2309699292F };
	V3F vg = V3F{ -1.2684380046F, 2.6097574011F, -0.3413193965F };
	V3F vb = V3F{ -0.0041960863F, -0.7034186147F, 1.7076147010F };
	V3F w{ w0, w1, w2 };
	V3F wSq{ w0 * w0, w1 * w1, w2 * w2 };
	V3F wCu{ w0 * w0 * w0, w1 * w1 * w1, w2 * w2 * w2 };
	// Create equations of the form L^3 + k0 * L^2C + k1 * LC^2 + K2 * C^3 == x
	// Where x is 0 or 1 for our purposes
	// 0 returning one or more lines instead of cubic curves because that's apparently guaranteed for equations of the form aX^3 + bX^2Y + cXY^2 + dY^3
	// L3 coeff is always 1 because all the v vectors sum to 1
	F32 kr0 = 3.0F * dot(vr, w); // L^2C
	F32 kr1 = 3.0F * dot(vr, wSq); // LC^2
	F32 kr2 = dot(vr, wCu); // C^3
	F32 kg0 = 3.0F * dot(vg, w); // L^2C
	F32 kg1 = 3.0F * dot(vg, wSq); // LC^2
	F32 kg2 = dot(vg, wCu); // C^3
	F32 kb0 = 3.0F * dot(vb, w); // L^2C
	F32 kb1 = 3.0F * dot(vb, wSq); // LC^2
	F32 kb2 = dot(vb, wCu); // C^3

	{ // Check if we're already in the srgb gamut
		F32 L = lch.x;
		F32 C = lch.y;
		F32 L3 = (L * L * L);
		F32 L2C = (L * L) * C;
		F32 LC2 = L * (C * C);
		F32 C3 = C * C * C;
		F32 r = L3 + kr0 * L2C + kr1 * LC2 + kr2 * C3;
		F32 g = L3 + kg0 * L2C + kg1 * LC2 + kg2 * C3;
		F32 b = L3 + kb0 * L2C + kb1 * LC2 + kb2 * C3;
		if (r >= 0.0F && r <= 1.0F && g >= 0.0F && g <= 1.0F && b >= 0.0F && b <= 1.0F) {
			return lrch;
		}
	}

	// Solve for C == 0.1 to give us the bottom bound line vector
	F32 C = 0.1F;
	F32 cubicResults[3];
	cubic_formula(cubicResults, 1.0F, kr0 * C, kr1 * (C * C), kr2 * (C * C * C));
	F32 lowerBoundLightness = cubicResults[2];
	cubic_formula(cubicResults, 1.0F, kg0 * C, kg1 * (C * C), kg2 * (C * C * C));
	lowerBoundLightness = max(lowerBoundLightness, cubicResults[2]);
	cubic_formula(cubicResults, 1.0F, kb0 * C, kb1 * (C * C), kb2 * (C * C * C));
	lowerBoundLightness = max(lowerBoundLightness, cubicResults[2]);

	// Substitute C = slope * L into each equation to solve for the cusp
	// This results in an equation of the form aL^3 - 1 == 0, which is easy to solve
	F32 slope = C / lowerBoundLightness;
	F32 ar = 1.0F + kr0 * slope + kr1 * (slope * slope) + kr2 * (slope * slope * slope);
	F32 ag = 1.0F + kg0 * slope + kg1 * (slope * slope) + kg2 * (slope * slope * slope);
	F32 ab = 1.0F + kb0 * slope + kb1 * (slope * slope) + kb2 * (slope * slope * slope);
	F32 cuspLightnessR = cbrtf32_robust(1.0F / ar);
	F32 cuspLightnessG = cbrtf32_robust(1.0F / ag);
	F32 cuspLightnessB = cbrtf32_robust(1.0F / ab);
	F32 cuspLightness = min(cuspLightnessR > 0.0F ? cuspLightnessR : F32_INF, cuspLightnessG > 0.0F ? cuspLightnessG : F32_INF, cuspLightnessB > 0.0F ? cuspLightnessB : F32_INF);
	F32 cuspChroma = slope * cuspLightness;

	// Compute adaptive compression point
	F32 alpha = 0.5F;
	F32 Ld = lch.x - 0.5F;
	F32 e1 = 0.5F + absf32(Ld) + alpha * lch.y;
	F32 L0 = 0.5F + 0.5F * F32(signumf32(Ld)) * (e1 - sqrtf32(e1 * e1 - 2.0F * absf32(Ld)));
	F32 C0 = 0.0F;

	V2F lchDir = V2F{ lch.y, lch.x - L0 };
	F32 resultL{};
	F32 resultC{};
	if (cross(lchDir, V2F{ cuspChroma, cuspLightness - L0 }) >= 0.0F) {
		// Intersect with linear lower bound
		F32 lchSlope = lchDir.y / lchDir.x;
		resultL = L0 / (1.0F - slope * lchSlope);
		resultC = resultL * slope;
	} else {
		// Intersect with cubic upper bound (need to evaluate all three)
		F32 o = L0;
		F32 o2 = o * o;
		F32 o3 = o2 * o;
		F32 x = lchDir.x;
		F32 x2 = x * x;
		F32 x3 = x2 * x;
		F32 y = lchDir.y;
		F32 y2 = y * y;
		F32 y3 = y2 * y;
		// L^3 + k0L^2C + k1LC^2 + k2C^3 = 1
		// tx = C, x = lchDir.x
		// o + ty = L, y = lchDir.y
		// (o + ty)^3 + k0(o + ty)^2(tx) + k1(o + ty)(tx)^2 + k2(tx)^3 = 1
		// o^3 + 3o^2yt + 3oy^2t^2 + y^3t^3 + k0xo^2t + 2k0xoyt^2 + k0xy^2t^3 + k1ox^2t^2 + k1yx^2t^3 + k2x^3t^3 = 1
		// (y^3 + k0xy^2 + k1yx^2 + k2x^3)t^3 + (3oy^2 + 2k0xoy + k1ox^2)t^2 + (3o^2y + k0xo^2)t + (o^3) = 1
		F32 a = y3 + kr0 * (x * y2) + kr1 * (y * x2) + kr2 * x3;
		F32 b = 3.0F * o * y2 + kr0 * (2.0F * x * o * y) + kr1 * (o * x2);
		F32 c = 3.0F * o2 * y + kr0 * (x * o2);
		F32 d = o3 - 1.0F;
		cubic_formula(cubicResults, a, b, c, d);
		F32 tr =
			cubicResults[0] >= 0.0F ? cubicResults[0] :
			cubicResults[1] >= 0.0F ? cubicResults[1] :
			cubicResults[2] >= 0.0F ? cubicResults[2] :
			F32_INF;

		a = y3 + kg0 * (x * y2) + kg1 * (y * x2) + kg2 * x3;
		b = 3.0F * o * y2 + kg0 * (2.0F * x * o * y) + kg1 * (o * x2);
		c = 3.0F * o2 * y + kg0 * (x * o2);
		cubic_formula(cubicResults, a, b, c, d);
		F32 tg =
			cubicResults[0] >= 0.0F ? cubicResults[0] :
			cubicResults[1] >= 0.0F ? cubicResults[1] :
			cubicResults[2] >= 0.0F ? cubicResults[2] :
			F32_INF;

		a = y3 + kb0 * (x * y2) + kb1 * (y * x2) + kb2 * x3;
		b = 3.0F * o * y2 + kb0 * (2.0F * x * o * y) + kb1 * (o * x2);
		c = 3.0F * o2 * y + kb0 * (x * o2);
		cubic_formula(cubicResults, a, b, c, d);
		F32 tb =
			cubicResults[0] >= 0.0F ? cubicResults[0] :
			cubicResults[1] >= 0.0F ? cubicResults[1] :
			cubicResults[2] >= 0.0F ? cubicResults[2] :
			F32_INF;

		F32 t = min(tr, tg, tb);
		resultC = x * t;
		resultL = o + y * t;
	}
	return V3F{ oklab_l_to_lightness(resultL), resultC, lrch.z };
}

}