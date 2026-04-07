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

V2F lc_cusp(F32 a, F32 b) {
	F32 S;
	if (-1.88170328F * a - 0.80936493F * b > 1.0F) {

	}
	return V2F{};
}
// This is broken because I don't have time to derive the clipping approximation right now
V3F clip_oklab_to_srgb_gamut(V3F Lab) {
	V3F lch = lab_to_lch(Lab);

	V2F lcCusp = lc_cusp(Lab.y, Lab.z);

	F32 a = 0.5F;
	F32 Ld = lch.x - 0.5F;
	F32 e1 = 0.5F + absf32(Ld) + a * lch.y;
	F32 L0 = 0.5F + 0.5F * F32(signumf32(Ld)) * (e1 - sqrtf32(e1 * e1 - 2.0F * absf32(Ld)));
	F32 C0 = 0.0F;
	return Lab;
}


V3F clip_oklab_to_srgb_gamut_lame(V3F lab) {
	V3F srgb = oklab_to_srgb(lab);
	srgb = clamp01(srgb);
	return srgb_to_oklab(srgb);
}
// Unfortunately, I haven't spent enough time on the math to figure out how to do good LrC clipping (and I'm not going to copy paste any code)
// This is very annoying because it means the clipping also changes the hue, but I'll just have to live with that until I derive the good clipping method
V3F clip_lrch_to_srgb_gamut_lame(V3F LrCH) {
	if (LrCH.y < 0.000001F) {
		// This is near perfect grey, try not to lose hue information
		// Clipping is not needed when chroma is zero
		return V3F{ LrCH.x, 0.0F, LrCH.z };
	} else {
		V3F Lab = lrch_to_lab(LrCH);
		V3F clippedLab = clip_oklab_to_srgb_gamut_lame(Lab);
		return lab_to_lrch(clippedLab);
	}
}

}