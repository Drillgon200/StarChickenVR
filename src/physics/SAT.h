#pragma once
#include "../DrillLib.h"
#include "RigidBody.h"

namespace SAT {
using namespace RigidBody;

bool is_intersecting(const OrientedBox& a, const OrientedBox& b) {
	M4x3F bToA;
	QF32 aGlobalToLocal = a.localToGlobalOrientation.conjugate();
	bToA.set_orientation_from_quat(aGlobalToLocal * b.localToGlobalOrientation);
	bToA.set_offset(aGlobalToLocal * (b.pos - a.pos));
	M4x3F aToB = bToA;
	aToB.invert_orthonormal();

	V3F axes[3 + 3 + 3 * 3];
	axes[0] = V3F{ 1.0F, 0.0F, 0.0F };
	axes[1] = V3F{ 0.0F, 1.0F, 0.0F };
	axes[2] = V3F{ 0.0F, 0.0F, 1.0F };
	axes[3] = bToA.transform_vec(V3F{ 1.0F, 0.0F, 0.0F });
	axes[4] = bToA.transform_vec(V3F{ 0.0F, 1.0F, 0.0F });
	axes[5] = bToA.transform_vec(V3F{ 0.0F, 0.0F, 1.0F });
	axes[6] = cross(V3F{ 1.0F, 0.0F, 0.0F }, axes[3]);
	axes[7] = cross(V3F{ 1.0F, 0.0F, 0.0F }, axes[4]);
	axes[8] = cross(V3F{ 1.0F, 0.0F, 0.0F }, axes[5]);
	axes[9] = cross(V3F{ 0.0F, 1.0F, 0.0F }, axes[3]);
	axes[10] = cross(V3F{ 0.0F, 1.0F, 0.0F }, axes[4]);
	axes[11] = cross(V3F{ 0.0F, 1.0F, 0.0F }, axes[5]);
	axes[12] = cross(V3F{ 0.0F, 0.0F, 1.0F }, axes[3]);
	axes[13] = cross(V3F{ 0.0F, 0.0F, 1.0F }, axes[4]);
	axes[14] = cross(V3F{ 0.0F, 0.0F, 1.0F }, axes[5]);

	for (U32 i = 0; i < ARRAY_COUNT(axes); i++) {
		V3F axis = axes[i];
		V2F aMinMax = a.project_minmax(V3F{}, axis);
		V2F bMinMax = b.project_minmax(aToB.transform_pos(V3F{}), aToB.transform_vec(axis));
		if (aMinMax.x > bMinMax.y || aMinMax.y < bMinMax.x) {
			return false;
		}
	}
	return true;
}

}