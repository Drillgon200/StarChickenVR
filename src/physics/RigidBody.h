#pragma once
#include "../DrillLib.h"
#include "../DynamicVertexBuffer.h"

namespace RigidBody {

struct OrientedBox {
	F32 minX, minY, minZ, maxX, maxY, maxZ;
	QF32 localToGlobalOrientation;
	V3F pos;

	V3F support(V3F dir) const {
		return V3F{ dir.x < 0.0F ? minX : maxX, dir.y < 0.0F ? minY : maxY, dir.z < 0.0F ? minZ : maxZ };
	}
	V2F project_minmax(V3F refPos, V3F dir) const {
		F32 max = dot(dir, support(dir) - refPos);
		F32 min = dot(dir, support(-dir) - refPos);
		return V2F{ min, max };
	}
	void debug_render(V3F color) {
		U32 packedColor = pack_unorm4x8(V4F{ color.x, color.y, color.z, 1.0F });
		VK::DebugVertex verts[8]{
			{ localToGlobalOrientation * V3F{ minX, minY, minZ } + pos, packedColor },
			{ localToGlobalOrientation * V3F{ maxX, minY, minZ } + pos, packedColor },
			{ localToGlobalOrientation * V3F{ minX, minY, maxZ } + pos, packedColor },
			{ localToGlobalOrientation * V3F{ maxX, minY, maxZ } + pos, packedColor },
			{ localToGlobalOrientation * V3F{ minX, maxY, minZ } + pos, packedColor },
			{ localToGlobalOrientation * V3F{ maxX, maxY, minZ } + pos, packedColor },
			{ localToGlobalOrientation * V3F{ minX, maxY, maxZ } + pos, packedColor },
			{ localToGlobalOrientation * V3F{ maxX, maxY, maxZ } + pos, packedColor }
		};
		U32 indices[12 * 2]{
			0, 1, 1, 3, 3, 2, 2, 0, // bottom square
			4, 5, 5, 7, 7, 6, 6, 4, // top square
			0, 4, 1, 5, 2, 6, 3, 7 // connector lines
		};
		using namespace DynamicVertexBuffer;
		Tessellator& tes = get_tessellator();
		tes.begin_draw(VK::debugLinesPipeline, DRAW_MODE_PRIMITIVES);
		tes.add_geometry(verts, ARRAY_COUNT(verts), indices, ARRAY_COUNT(indices));
		tes.end_draw();
	}
};

}