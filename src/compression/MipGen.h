#pragma once

#include "../DrillLib.h"

namespace MipGen {

DEBUG_OPTIMIZE_ON

// Thanks to Jon Blow for the name
RGBA8* build_lame_mipmaps(MemoryArena& arena, U32* elementCountOut, U32* countOut, RGBA8* src, U32 width, U32 height, bool srgb) {
	RUNTIME_ASSERT(is_power_of_2(U64(width)) && is_power_of_2(U64(height)), "Only supporting power of 2 for simplicity"a);

	arena.stackPtr = ALIGN_HIGH(arena.stackPtr, 4);
	U64 prevArenaStackPtr = arena.stackPtr;
	RGBA8* result = arena.alloc<RGBA8>(width * height * 2);
	memcpy(result, src, width * height * sizeof(RGBA8));
	V4F* intermediate = arena.alloc<V4F>(width * height * 2);

	// Convert to F32 precision so we don't repeatedly quantize/sRGB compress, just in case that causes something bad
	for (U32 i = 0; i < width * height; i++) {
		intermediate[i] = srgb ? from_srgb(src[i].to_v4f32()) : src[i].to_v4f32();
	}
	V4F* prevPtr = intermediate;
	V4F* nextPtr = intermediate + width * height;
	RGBA8* resultPtr = result + width * height;
	U32 mipCount = 1;
	while (width > 1 || height > 1) {
		U32 prevWidth = width;
		width = max(1u, width >> 1);
		height = max(1u, height >> 1);
		mipCount++;
		for (U32 y = 0; y < height; y++) {
			for (U32 x = 0; x < width; x++) {
				V4F filtered =
					(prevPtr[(y * 2 + 0) * prevWidth + x * 2 + 0] +
					prevPtr[(y * 2 + 0) * prevWidth + x * 2 + 1] +
					prevPtr[(y * 2 + 1) * prevWidth + x * 2 + 0] +
					prevPtr[(y * 2 + 1) * prevWidth + x * 2 + 1]) * 0.25F;
				nextPtr[y * width + x] = filtered;
				resultPtr[y * width + x] = (srgb ? to_srgb(filtered) : filtered).to_rgba8();
			}
		}
		prevPtr = nextPtr;
		nextPtr += width * height;
		resultPtr += width * height;
	}

	U32 totalElementCount = U32(resultPtr - result);
	if (elementCountOut) {
		*elementCountOut = totalElementCount;
	}
	arena.stackPtr = prevArenaStackPtr + totalElementCount * sizeof(RGBA8);
	*countOut = mipCount;
	return result;
}

DEBUG_OPTIMIZE_OFF

}