#pragma once
#include "DrillLib.h"
#include "DynamicVertexBuffer_decl.h"
#include "VK.h"

namespace DynamicVertexBuffer {

struct DrawCommand {
	VkPipeline pipeline;
	U32 vertexBufferOffset;
	U32 indexBufferOffset;
	U32 vertexCount;
	U32 indexCount;
	U64 clipBoxesGPUAddress;
};

enum DrawMode : U32 {
	DRAW_MODE_PRIMITIVES,
	DRAW_MODE_QUADS
};

struct Tessellator {
	VK::DedicatedBuffer buffer;
	U32 vertexCapacity;
	U32 indexCapacity;
	U32 currentVertexByteCount;
	U32 currentIndexByteCount;
	Byte* vertexDataPointer;
	U32* indexDataPointer;
	B32 isCurrentlyDrawing;
	DrawMode currentDrawMode;
	U32 lastDrawSetCmdIdx;
	ArenaArrayList<DrawCommand> drawCommands;

	void ensure_space_for(U32 vertexBytes, U32 indexBytes) {
		if (currentVertexByteCount + vertexBytes <= vertexCapacity && currentIndexByteCount + indexBytes <= indexCapacity) {
			return;
		}
		VK::DedicatedBuffer oldBuffer = buffer;
		Byte* oldVertexDataPointer = vertexDataPointer;
		U32* oldIndexDataPointer = indexDataPointer;

		vertexCapacity = next_power_of_two(currentVertexByteCount + vertexBytes);
		indexCapacity = next_power_of_two(currentIndexByteCount + indexBytes);

		buffer.create(vertexCapacity + indexCapacity, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK::hostMemoryTypeIndex);
		vertexDataPointer = reinterpret_cast<Byte*>(buffer.mapping);
		indexDataPointer = reinterpret_cast<U32*>(vertexDataPointer + vertexCapacity);

		if (oldVertexDataPointer) {
			memcpy(vertexDataPointer, oldVertexDataPointer, currentVertexByteCount);
			memcpy(indexDataPointer, oldIndexDataPointer, currentIndexByteCount);
			oldBuffer.destroy();
		}
	}

	void init() {
		buffer = {};
		vertexDataPointer = nullptr;
		indexDataPointer = nullptr;
		currentVertexByteCount = 0;
		currentIndexByteCount = 0;
		isCurrentlyDrawing = false;
		drawCommands = ArenaArrayList<DrawCommand>{};
		ensure_space_for(4 * MEGABYTE, 1 * MEGABYTE);

	}
	void destroy() {
		buffer.destroy();
	}

	VkDeviceAddress get_gpu_address() {
		return buffer.gpuAddress;
	}

	void begin_draw(VkPipeline pipeline, DrawMode mode) {
		if (isCurrentlyDrawing) {
			abort("Already drawing something, end that draw first");
		}
		isCurrentlyDrawing = true;
		currentDrawMode = mode;
		drawCommands.allocator = &frameArena;
		currentVertexByteCount = ALIGN_HIGH(currentVertexByteCount, 16);
		DrawCommand& drawCmd = drawCommands.push_back();
		drawCmd.pipeline = pipeline;
		drawCmd.indexBufferOffset = currentIndexByteCount / sizeof(U32);
		drawCmd.vertexBufferOffset = currentVertexByteCount;
		drawCmd.vertexCount = 0;
		drawCmd.indexCount = 0;
		drawCmd.clipBoxesGPUAddress = 0;
	}
	void set_clip_boxes(U64 address) {
		drawCommands.back().clipBoxesGPUAddress = address;
	}
	void end_draw() {
		isCurrentlyDrawing = false;
	}
	Rng1I32 end_draw_set() {
		Rng1I32 result{ I32(lastDrawSetCmdIdx), I32(drawCommands.size) };
		lastDrawSetCmdIdx = drawCommands.size;
		return result;
	}
	void draw(Rng1I32 drawRange, U32 camIdx) {
		VK::DrawPushData renderData{ .drawSet = VK::defaultDrawDescriptorSet };
		renderData.drawConstants.camIdx = camIdx;
		VK::vkCmdBindIndexBuffer(VK::graphicsCommandBuffer, buffer.buffer, vertexCapacity, VK_INDEX_TYPE_UINT32);
		VK_PUSH_MEMBER(VK::graphicsCommandBuffer, renderData, drawSet);
		VkPipeline prevPipeline = VK_NULL_HANDLE;
		for (I32 drawCmdIdx = drawRange.minX; drawCmdIdx < drawRange.maxX; drawCmdIdx++) {
			DrawCommand& drawCmd = drawCommands.data[drawCmdIdx];
			if (drawCmd.pipeline != prevPipeline) {
				VK::vkCmdBindPipeline(VK::graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, drawCmd.pipeline);
				prevPipeline = drawCmd.pipeline;
			}
			renderData.drawConstants.verticesOffset = I32(drawCmd.vertexBufferOffset);
			VK_PUSH_MEMBER(VK::graphicsCommandBuffer, renderData, drawConstants);
			VK::vkCmdDrawIndexed(VK::graphicsCommandBuffer, drawCmd.indexCount, 1, drawCmd.indexBufferOffset, 0, 0);
		}
	}

	void end_frame() {
		buffer.invalidate_mapped();
		drawCommands = ArenaArrayList<DrawCommand>{ &frameArena };
		currentIndexByteCount = 0;
		currentVertexByteCount = 0;
		lastDrawSetCmdIdx = 0;
	}

	Tessellator& pos3(F32 x, F32 y, F32 z) {
		STORE_LE32(vertexDataPointer + currentVertexByteCount, bitcast<U32>(x));
		STORE_LE32(vertexDataPointer + currentVertexByteCount + 4, bitcast<U32>(y));
		STORE_LE32(vertexDataPointer + currentVertexByteCount + 8, bitcast<U32>(z));
		currentVertexByteCount += 3 * sizeof(F32);
		return *this;
	}
	Tessellator& pos2(F32 x, F32 y) {
		STORE_LE32(vertexDataPointer + currentVertexByteCount, bitcast<U32>(x));
		STORE_LE32(vertexDataPointer + currentVertexByteCount + 4, bitcast<U32>(y));
		currentVertexByteCount += 2 * sizeof(F32);
		return *this;
	}
	Tessellator& tex(F32 u, F32 v) {
		STORE_LE32(vertexDataPointer + currentVertexByteCount, bitcast<U32>(u));
		STORE_LE32(vertexDataPointer + currentVertexByteCount + 4, bitcast<U32>(v));
		currentVertexByteCount += 2 * sizeof(F32);
		return *this;
	}
	Tessellator& color(F32 r, F32 g, F32 b, F32 a) {
		U32 color = pack_unorm4x8(V4F32{ r, g, b, a });
		STORE_LE32(vertexDataPointer + currentVertexByteCount, color);
		currentVertexByteCount += sizeof(U32);
		return *this;
	}
	Tessellator& color(F32 r, F32 g, F32 b) {
		return color(r, g, b, 1.0F);
	}
	Tessellator& u32(U32 v) {
		STORE_LE32(vertexDataPointer + currentVertexByteCount, v);
		currentVertexByteCount += sizeof(U32);
		return *this;
	}
	Tessellator& tex_idx(U32 v) {
		return u32(v);
	}
	Tessellator& flags(U32 v) {
		return u32(v);
	}
	template<typename T>
	Tessellator& element(T& element) {
		memcpy(vertexDataPointer + currentVertexByteCount, &element, sizeof(element));
		currentVertexByteCount += sizeof(element);
		return *this;
	}
	Tessellator& end_vert() {
		DrawCommand& cmd = drawCommands.back();
		cmd.vertexCount++;
		U32 indexOffset = currentIndexByteCount / 4;
		if (currentDrawMode == DRAW_MODE_QUADS) {
			if ((cmd.vertexCount & 0b11) == 0) {
				indexDataPointer[indexOffset] = cmd.vertexCount - 4;
				indexDataPointer[indexOffset + 1] = cmd.vertexCount - 3;
				indexDataPointer[indexOffset + 2] = cmd.vertexCount - 2;
				indexDataPointer[indexOffset + 3] = cmd.vertexCount - 4;
				indexDataPointer[indexOffset + 4] = cmd.vertexCount - 2;
				indexDataPointer[indexOffset + 5] = cmd.vertexCount - 1;
				cmd.indexCount += 6;
				currentIndexByteCount += 6 * sizeof(U32);
			}
		} else { // Other draw modes don't need to be tessellated
			indexDataPointer[indexOffset] = cmd.indexCount++;
			currentIndexByteCount += sizeof(U32);
		}
		ensure_space_for(1024, 6 * sizeof(U32));
		return *this;
	}
	template<typename Vertex>
	Tessellator& add_geometry(Vertex* vertices, U32 numVerts, U32* indices, U32 numIndices) {
		DrawCommand& cmd = drawCommands.back();
		ensure_space_for(numVerts * sizeof(Vertex), numIndices * sizeof(U32));
		memcpy(vertexDataPointer + currentVertexByteCount, vertices, numVerts * sizeof(Vertex));
		U32 indexOffset = currentIndexByteCount / 4;
		for (U32 i = 0; i < numIndices; i++) {
			indexDataPointer[indexOffset + i] = indices[i] + cmd.vertexCount;
		}
		currentVertexByteCount += numVerts * sizeof(Vertex);
		currentIndexByteCount += numIndices * sizeof(U32);
		cmd.vertexCount += numVerts;
		cmd.indexCount += numIndices;
		return *this;
	}
	Tessellator& ui_rect2d(F32 xStart, F32 yStart, F32 xEnd, F32 yEnd, F32 z, F32 uStart, F32 vStart, F32 uEnd, F32 vEnd, V4F32 color, U32 textureIndex, U32 flags) {

		DrawCommand& cmd = drawCommands.back();
		ensure_space_for(4 * sizeof(VK::UIVertex), 6 * sizeof(U32));

		U32 packedColor = pack_unorm4x8(color);
		VK::UIVertex vertices[4]{
			{ V3F32{ xStart, yStart, z }, V2F32{ uStart, vStart }, packedColor, textureIndex, flags },
			{ V3F32{ xStart, yEnd, z }, V2F32{ uStart, vEnd }, packedColor, textureIndex, flags },
			{ V3F32{ xEnd, yEnd, z }, V2F32{ uEnd, vEnd }, packedColor, textureIndex, flags },
			{ V3F32{ xEnd, yStart, z }, V2F32{ uEnd, vStart }, packedColor, textureIndex, flags },
		};

		memcpy(vertexDataPointer + currentVertexByteCount, vertices, 4 * sizeof(VK::UIVertex));
		U32 indexOffset = currentIndexByteCount / 4;
		indexDataPointer[indexOffset + 0] = cmd.vertexCount + 0;
		indexDataPointer[indexOffset + 1] = cmd.vertexCount + 1;
		indexDataPointer[indexOffset + 2] = cmd.vertexCount + 2;
		indexDataPointer[indexOffset + 3] = cmd.vertexCount + 0;
		indexDataPointer[indexOffset + 4] = cmd.vertexCount + 2;
		indexDataPointer[indexOffset + 5] = cmd.vertexCount + 3;

		currentVertexByteCount += 4 * sizeof(VK::UIVertex);
		currentIndexByteCount += 6 * sizeof(U32);
		cmd.vertexCount += 4;
		cmd.indexCount += 6;
		return *this;
	}
	Tessellator& ui_line_strip(V2F32* points, U32 pointCount, F32 z, F32 thickness, V4F32 color, U32 textureIndex, U32 flags) {
		if (pointCount < 2) {
			return *this;
		}
		DrawCommand& cmd = drawCommands.back();
		U32 packedColor = pack_unorm4x8(color);
		U32 vertexCount = pointCount * 2;
		U32 indexCount = pointCount * 6 - 6;
		F32 scale = thickness * 0.5F;
		MemoryArena& stackArena = get_scratch_arena();
		MEMORY_ARENA_FRAME(stackArena) {
			VK::UIVertex* vertices = stackArena.alloc<VK::UIVertex>(vertexCount);
			// There's a better shape I should be using here that uses 4 tris per segment and mitigates a lot of distortion
			V2F32 toNext = normalize(points[1] - points[0]);
			V2F32 toPrev{};
			V2F32 orthogonal = get_orthogonal(toNext) * scale;
			V2F32 tex0{ 0.0F, 0.0F };
			V2F32 tex1{ 0.0F, 1.0F };
			V2F32 pos0 = points[0] - orthogonal;
			V2F32 pos1 = points[0] + orthogonal;
			vertices[0] = VK::UIVertex{ V3F32{ pos0.x, pos0.y, z}, tex0, packedColor, textureIndex, flags };
			vertices[1] = VK::UIVertex{ V3F32{ pos1.x, pos1.y, z}, tex1, packedColor, textureIndex, flags };
			for (U32 pointIdx = 1, vertIdx = 2; pointIdx < pointCount - 1; pointIdx++, vertIdx += 2) {
				V2F32 pos = points[pointIdx];
				toNext = normalize(points[pointIdx + 1] - pos);
				toPrev = normalize(points[pointIdx - 1] - pos);
				V2F32 direction = normalize(toNext + toPrev);
				F32 scaleA = 0.0F;
				F32 scaleB = 0.0F;
				if (absf32(cross(toNext, toPrev)) < 0.0001F) {
					direction = V2F32{ -toNext.y, toNext.x };
					scaleA = scaleB = scale;
				} else {
					scaleA = scale / absf32(dot(toNext, get_orthogonal(direction)));
					scaleA = min(scaleA, length(points[pointIdx - 1] - pos), length(points[pointIdx + 1] - pos));
					scaleB = scale / max(absf32(dot(toNext, get_orthogonal(direction))), 0.2F);
				}
				if (cross(direction, toNext) < 0.0F) {
					direction = -direction;
					swap(&scaleA, &scaleB);
				}

				F32 t = F32(pointIdx) / F32(pointCount - 1);
				tex0.x = t;
				tex1.x = t;
				pos0 = pos + direction * scaleA;
				pos1 = pos - direction * scaleB;
				vertices[vertIdx + 0] = VK::UIVertex{ V3F32{ pos0.x, pos0.y, z}, tex0, packedColor, textureIndex, flags };
				vertices[vertIdx + 1] = VK::UIVertex{ V3F32{ pos1.x, pos1.y, z}, tex1, packedColor, textureIndex, flags };
			}
			toNext = normalize(points[pointCount - 1] - points[pointCount - 2]);
			orthogonal = V2F32{ -toNext.y * scale, toNext.x * scale };
			tex0.x = 1.0F;
			tex1.x = 1.0F;
			pos0 = points[pointCount - 1] - orthogonal;
			pos1 = points[pointCount - 1] + orthogonal;
			vertices[vertexCount - 2] = VK::UIVertex{ V3F32{ pos0.x, pos0.y, z}, tex0, packedColor, textureIndex, flags };
			vertices[vertexCount - 1] = VK::UIVertex{ V3F32{ pos1.x, pos1.y, z}, tex1, packedColor, textureIndex, flags };

			ensure_space_for(vertexCount * sizeof(VK::UIVertex), indexCount * sizeof(U32));

			memcpy(vertexDataPointer + currentVertexByteCount, vertices, vertexCount * sizeof(VK::UIVertex));
			U32 indexOffset = currentIndexByteCount / 4;
			U32* indexPtr = indexDataPointer + indexOffset;
			for (U32 i = 0, vertexIdx = cmd.vertexCount; i < indexCount; i += 6, vertexIdx += 2) {
				indexPtr[i + 0] = vertexIdx + 0;
				indexPtr[i + 1] = vertexIdx + 3;
				indexPtr[i + 2] = vertexIdx + 2;
				indexPtr[i + 3] = vertexIdx + 0;
				indexPtr[i + 4] = vertexIdx + 1;
				indexPtr[i + 5] = vertexIdx + 3;
			}
			currentVertexByteCount += vertexCount * sizeof(VK::UIVertex);
			currentIndexByteCount += indexCount * sizeof(U32);
			cmd.vertexCount += vertexCount;
			cmd.indexCount += indexCount;
		}
		return *this;
	}
	Tessellator& ui_bezier_curve(V2F32 start, V2F32 controlA, V2F32 controlB, V2F32 end, F32 z, U32 subdivisions, F32 thickness, V4F32 color, U32 textureIndex, U32 flags) {
		subdivisions = max(subdivisions, 2u);
		MemoryArena& stackArena = get_scratch_arena();
		MEMORY_ARENA_FRAME(stackArena) {
			V2F32* positions = stackArena.alloc<V2F32>(subdivisions);
			for (U32 i = 0; i < subdivisions; i++) {
				F32 t = F32(i) / F32(subdivisions - 1);
				positions[i] = eval_bezier_cubic(start, controlA, controlB, end, t);
			}
			ui_line_strip(positions, subdivisions, z, thickness, color, textureIndex, flags);
		}
		return *this;
	}
	
	// Unfinished, will not work
	Tessellator& debug_line(V3F start, V3F end, V4F color = V4F{ 1.0F, 0.0F, 0.0F, 1.0F }) {
		V3F v = end - start;
		F32 dist = length(v);
		v /= dist;
		M4x3F t;
		t.x = start.x; t.y = start.y; t.z = start.z;
		// We will construct the line along the y axis, so align the y axis with the correct direction, and the other two basis vectors don't matter as long as they're orthonormal
		V3F b2 = normalize(cross(v, V3F{ 0.0F, 1.0F, 0.0F }));
		V3F b1 = cross(b2, v);
		if (v.y > 0.999F) {
			b1 = V3F{ 1.0F, 0.0F, 0.0F };
			b2 = V3F{ 0.0F, 0.0F, 1.0F };
		} else if (v.y < -0.999F) {
			b1 = V3F{ -1.0F, 0.0F, 0.0F };
			b2 = V3F{ 0.0F, 0.0F, 1.0F };
		}
		t.m00 = b1.x; t.m01 = b1.y; t.m02 = b1.z;
		t.m10 = v.x;  t.m11 = v.y;  t.m12 = v.z;
		t.m20 = b2.x; t.m21 = b2.y; t.m22 = b2.z;

		V3F vertices[8]{
			t * V3F{ -1.0F, 0.0F, -1.0F },
			t * V3F{ 1.0F, 0.0F, -1.0F },
			t * V3F{ 1.0F, 0.0F, 1.0F },
			t * V3F{ -1.0F, 0.0F, 1.0F },
			t * V3F{ -1.0F, dist, -1.0F },
			t * V3F{ 1.0F, dist, -1.0F },
			t * V3F{ 1.0F, dist, 1.0F },
			t * V3F{ -1.0F, dist, 1.0F }
		};
		U32 indices[6 * 4]{
			0, 1, 5, 0, 5, 4,
			1, 2, 6, 1, 6, 5,
			2, 3, 7, 2, 7, 6,
			3, 0, 4, 3, 4, 7
		};
		return add_geometry(vertices, 8, indices, 6 * 4);
	}
	Tessellator& debug_arrow(V3F start, V3F end, F32 arrowRadius, V4F color = V4F{ 1.0F, 0.0F, 0.0F, 1.0F }) {
		V3F v = end - start;
		F32 dist = length(v);
		v /= dist;
		M4x3F t;
		t.x = start.x; t.y = start.y; t.z = start.z;
		// We will construct the line along the y axis, so align the y axis with the correct direction, and the other two basis vectors don't matter as long as they're orthonormal
		// https://box2d.org/posts/2014/02/computing-a-basis/
		V3F b1;
		if (absf32(v.x) > 0.57735F) {
			b1 = V3F{ v.y, -v.x, 0.0F };
		} else {
			b1 = V3F{ 0.0F, v.z, -v.y };
		}
		b1 = normalize(b1) * arrowRadius;
		V3F b2 = cross(v, b1);
		t.m00 = b1.x; t.m10 = b1.y; t.m20 = b1.z;
		t.m01 = v.x;  t.m11 = v.y;  t.m21 = v.z;
		t.m02 = b2.x; t.m12 = b2.y; t.m22 = b2.z;

		F32 headHeight = arrowRadius * 4.0F;
		dist = max(0.0F, dist - headHeight);

		U32 packedColor = pack_unorm4x8(color);
		// I don't need this to be super high fidelity, so I'm hard coding an octagonal prism
		VK::DebugVertex vertices[]{
			// Body vertices
			{ t * V3F{ 1.0F, 0.0F, 0.0F }, packedColor },
			{ t * V3F{ 0.707F, 0.0F, 0.707F }, packedColor },
			{ t * V3F{ 0.0F, 0.0F, 1.0F }, packedColor },
			{ t * V3F{ -0.707F, 0.0F, 0.707F }, packedColor },
			{ t * V3F{ -1.0F, 0.0F, 0.0F }, packedColor },
			{ t * V3F{ -0.707F, 0.0F, -0.707F }, packedColor },
			{ t * V3F{ 0.0F, 0.0F, -1.0F }, packedColor },
			{ t * V3F{ 0.707F, 0.0F, -0.707F }, packedColor },
			{ t * V3F{ 1.0F, dist, 0.0F }, packedColor },
			{ t * V3F{ 0.707F, dist, 0.707F }, packedColor },
			{ t * V3F{ 0.0F, dist, 1.0F }, packedColor },
			{ t * V3F{ -0.707F, dist, 0.707F }, packedColor },
			{ t * V3F{ -1.0F, dist, 0.0F }, packedColor },
			{ t * V3F{ -0.707F, dist, -0.707F }, packedColor },
			{ t * V3F{ 0.0F, dist, -1.0F }, packedColor },
			{ t * V3F{ 0.707F, dist, -0.707F }, packedColor },
			// Head vertices
			{ t * V3F{ 2.0F, dist, 0.0F }, packedColor },
			{ t * V3F{ 1.414F, dist, 1.414F }, packedColor },
			{ t * V3F{ 0.0F, dist, 2.0F }, packedColor },
			{ t * V3F{ -1.414F, dist, 1.414F }, packedColor },
			{ t * V3F{ -2.0F, dist, 0.0F }, packedColor },
			{ t * V3F{ -1.414F, dist, -1.414F }, packedColor },
			{ t * V3F{ 0.0F, dist, -2.0F }, packedColor },
			{ t * V3F{ 1.414F, dist, -1.414F }, packedColor },
			{ t * V3F{ 0.0F, dist + headHeight, 0.0F }, packedColor }
		};
		U32 indices[]{
			// Cylinder sides
			0, 1, 9, 0, 9, 8,
			1, 2, 10, 1, 10, 9,
			2, 3, 11, 2, 11, 10,
			3, 4, 12, 3, 12, 11,
			4, 5, 13, 4, 13, 12,
			5, 6, 14, 5, 14, 13,
			6, 7, 15, 6, 15, 14,
			7, 0, 8, 7, 8, 15,
			// Cylinder bottom
			0, 7, 6, 0, 6, 5, 0, 5, 4, 0, 4, 3, 0, 3, 2, 0, 2, 1,
			// Head to cylinder connecting geometry
			8, 9, 17, 8, 17, 16,
			9, 10, 18, 9, 18, 17,
			10, 11, 19, 10, 19, 18,
			11, 12, 20, 11, 20, 19,
			12, 13, 21, 12, 21, 20,
			13, 14, 22, 13, 22, 21,
			14, 15, 23, 14, 23, 22,
			15, 8, 16, 15, 16, 23,
			// Head
			16, 17, 24, 17, 18, 24, 18, 19, 24, 19, 20, 24, 20, 21, 24, 21, 22, 24, 22, 23, 24, 23, 16, 24
		};
		return add_geometry(vertices, ARRAY_COUNT(vertices), indices, ARRAY_COUNT(indices));
	}
} tessellators[VK::FRAMES_IN_FLIGHT];

void init() {
	for (U32 i = 0; i < VK::FRAMES_IN_FLIGHT; i++) {
		tessellators[i].init();
	}
}
void destroy() {
	for (U32 i = 0; i < VK::FRAMES_IN_FLIGHT; i++) {
		tessellators[i].destroy();
	}
}

Tessellator& get_tessellator() {
	return tessellators[VK::currentFrameInFlight];
}
// This function seems useless, but it avoids a circular dependency on VK.h
VkDeviceAddress get_gpu_address(Tessellator& t) {
	return t.buffer.gpuAddress;
}

}