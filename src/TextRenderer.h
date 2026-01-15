#pragma once
#include "DrillLib.h"
#include "SerializeTools.h"
#include "DynamicVertexBuffer.h"
#include "Resources.h"

namespace TextRenderer {

F32 string_size_x(StrA str, F32 sizeY) {
	const F32 characterSpacing = 1.0F;
	const F32 fontCharacterWidth = 5.0F;
	const F32 fontCharacterHeight = 12.0F;

	F32 sizeX = sizeY * (fontCharacterWidth / fontCharacterHeight);
	F32 characterStep = sizeX + characterSpacing / fontCharacterWidth * sizeX;
	F32 maxSize = 0.0F;
	for (U64 i = 0; i < str.length; i++) {
		U64 prev = i;
		for (; str.str[i] != '\n' && i < str.length; i++);
		maxSize = max(maxSize, characterStep * F32(i - prev));
	}
	return maxSize;
}

F32 get_character_size(char c, F32 sizeY) {
	const F32 characterSpacing = 1.0F;
	const F32 fontCharacterWidth = 5.0F;
	const F32 fontCharacterHeight = 12.0F;

	F32 sizeX = sizeY * (fontCharacterWidth / fontCharacterHeight);
	F32 characterStep = sizeX + characterSpacing / fontCharacterWidth * sizeX;
	return characterStep;
}

F32 string_size_y(StrA str, F32 sizeY) {
	const F32 lineSpacing = 0.0F;

	U64 lineCount = 1;
	for (U64 i = 0; i < str.length; i++) {
		lineCount += str.str[i] == '\n';
	}
	return F32(lineCount) * (sizeY + lineSpacing);
}

StrA wrap_text(MemoryArena& arena, StrA str, F32 width, F32 sizeY) {
	using namespace SerializeTools;
	const F32 characterSpacing = 1.0F;
	const F32 fontCharacterWidth = 5.0F;
	const F32 fontCharacterHeight = 12.0F;

	F32 sizeX = sizeY * (fontCharacterWidth / fontCharacterHeight);
	F32 characterStep = sizeX + characterSpacing / fontCharacterWidth * sizeX;

	char* result = arena.alloc<char>(0);
	char* writePtr = result;
	// Wrap text by cutting it at either the space before the word the crosses the size boundry or the character that does. Make sure to put at least one character per line
	F32 current = 0.0F;
	for (U64 i = 0; i < str.length; i++) {
		char c = str.str[i];
		if (c == '\n') {
			*writePtr++ = '\n';
			current = 0.0F;
			continue;
		}
		current += characterStep;
		if (current > width) {
			*writePtr++ = '\n';
			current = characterStep;
		}
		*writePtr++ = c;
		// Attempt to insert extra newlines after spaces to keep words from splitting
		if (is_whitespace(c)) {
			F32 speculativeWordSize = current;
			U64 j = i + 1;
			// Treat a block of characters + a block of whitespace as one unit
			for (; j < str.length && !is_whitespace(str.str[j]); j++) {
				speculativeWordSize += characterStep;
			}
			for (; j < str.length && is_whitespace(str.str[j]); j++) {
				speculativeWordSize += characterStep;
			}
			if (speculativeWordSize > width) {
				*writePtr++ = '\n';
				current = 0.0F;
			}
		}
	}
	U64 length = U64(writePtr - result);
	arena.stackPtr += length;
	return StrA{ result, length };
}

void draw_string_batched(DynamicVertexBuffer::Tessellator& tes, StrA str, F32 x, F32 y, F32 z, F32 sizeY, V4F32 color, U32 flags) {
	// Font characters are assumed to be 12 units tall and 5 units wide
	// I'm only going to handle monospaced right now because it's easier and I like monospaced fonts more anyway

	//TODO don't hard code this
	const F32 atlasCharacterResolution = 64.0F;
	const F32 sdfPxRadius = 12.0F;
	const F32 fontCharacterWidth = 5.0F;
	const F32 fontCharacterHeight = 12.0F;
	const F32 characterSpacing = 1.0F;
	const F32 lineSpacing = 0.0F;

	F32 radius = sdfPxRadius / atlasCharacterResolution;
	F32 height = 1.0F;
	F32 width = fontCharacterWidth / fontCharacterHeight;
	F32 offsetX = (16.0F / atlasCharacterResolution);
	F32 offsetY = 0.0F;
	F32 widthWithinAtlas = width * (1.0F / 16.0F);
	F32 heightWithinAtlas = height * (1.0F / 16.0F);
	F32 sizeX = sizeY * (fontCharacterWidth / fontCharacterHeight);
	F32 dstRadius = radius * sizeY;
	F32 characterStep = sizeX + characterSpacing / fontCharacterWidth * sizeX;
	F32 lineStep = sizeY + lineSpacing;

	x -= dstRadius;
	y -= dstRadius;
	sizeX += dstRadius * 2.0F;
	sizeY += dstRadius * 2.0F;

	VK::UIVertex vertices[4];
	U32 quadIndices[6]{ 0, 1, 2, 0, 2, 3 };
	U32 packedColor = pack_unorm4x8(color);
	for (U32 i = 0; i < ARRAY_COUNT(vertices); i++) {
		vertices[i].color = packedColor;
		vertices[i].texIdx = Resources::fontAtlas.index;
		vertices[i].flags = flags | VK::UI_RENDER_FLAG_MSDF;
	}

	F32 xBegin = x;
	for (U64 i = 0; i < str.length; i++) {
		if (str.str[i] == '\n') {
			y += lineStep;
			x = xBegin;
			continue;
		}
		U32 glyph = U32(str.str[i]);
		F32 xStart = (F32(glyph & 0xF) + offsetX) * (1.0F / 16.0F);
		F32 yStart = (F32(glyph >> 4) + offsetY) * (1.0F / 16.0F);
		vertices[0].tex = V2F32{ xStart, yStart };
		vertices[1].tex = V2F32{ xStart, yStart + heightWithinAtlas };
		vertices[2].tex = V2F32{ xStart + widthWithinAtlas, yStart + heightWithinAtlas };
		vertices[3].tex = V2F32{ xStart + widthWithinAtlas, yStart };
		vertices[0].pos = V3F32{ x, y, z };
		vertices[1].pos = V3F32{ x, y + sizeY, z };
		vertices[2].pos = V3F32{ x + sizeX, y + sizeY, z };
		vertices[3].pos = V3F32{ x + sizeX, y, z };
		tes.add_geometry(vertices, 4, quadIndices, 6);

		x += characterStep;
	}
}

void draw_string(StrA str, F32 x, F32 y, F32 z, F32 sizeY, V4F32 color, U32 flags) {
	DynamicVertexBuffer::Tessellator& tes = DynamicVertexBuffer::get_tessellator();
	tes.begin_draw(VK::uiPipeline, VK::drawPipelineLayout, DynamicVertexBuffer::DRAW_MODE_QUADS);
	draw_string_batched(tes, str, x, y, z, sizeY, color, flags);
	tes.end_draw();
}

void draw_string(StrA str, F32 x, F32 y, F32 z, F32 sizeY) {
	draw_string(str, x, y, z, sizeY, V4F32{ 1.0F, 1.0F, 1.0F, 1.0F }, 0);
}

}