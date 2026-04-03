#pragma once

#include "../DrillLib.h"

namespace MipGen {

// Thanks to Jon Blow for the name
RGBA8* build_lame_mipmaps(MemoryArena& arena, U32* countOut, RGBA8* src, U32 width, U32 height) {
	RUNTIME_ASSERT(is_power_of_2(U64(width)) && is_power_of_2(U64(height)), "Only supporting power of 2 for simplicity"a);
	arena.stackPtr = ALIGN_HIGH(arena.stackPtr, 4);
	RGBA8* result = arena.alloc<RGBA8>(width * height);
	memcpy(result, src, width * height * sizeof(RGBA8));
	RGBA8* prev = src;
	U32 mipCount = 1;
	while (width > 1 || height > 1) {
		U32 prevWidth = width;
		width = max(1u, width >> 1);
		height = max(1u, height >> 1);
		RGBA8* next = arena.alloc<RGBA8>(width * height);
		mipCount++;
		for (U32 y = 0; y < height; y++) {
			for (U32 x = 0; x < width; x++) {
				// We should do the SRGB conversion but I don't have pow implemented right now so we're doing it the even lamer way.
				V4F filtered =
					prev[(y * 2 + 0) * prevWidth + x * 2 + 0].to_v4f32() +
					prev[(y * 2 + 0) * prevWidth + x * 2 + 1].to_v4f32() +
					prev[(y * 2 + 1) * prevWidth + x * 2 + 0].to_v4f32() +
					prev[(y * 2 + 1) * prevWidth + x * 2 + 1].to_v4f32();
				next[y * width + x] = (filtered * 0.25F).to_rgba8();
			}
		}
		prev = next;
	}
	*countOut = mipCount;
	return result;
}

}