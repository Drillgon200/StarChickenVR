#pragma once
#include "../DrillLib.h"
#include "../VK.h"
#include "../DynamicVertexBuffer.h"

namespace Physics {

struct Point {
	V3F pos;
	V3F prevPos;
	// Using an inertial velocity instead of position improves precision significantly at low timesteps
	V3F inertialVel;
	//V3F inertialPos;
	V3F vel;
	F32 mass;
	U32 constraintCount;
	U32 constraintEndOffset;
	U32 graphColor;
};

struct RigidBody {
	V3F pos;
	// Axis angle format where the angle is the length of the axis.
	// The angle is in radians instead of turns because it represents the distance a point at unit distance is rotated.
	V3F rotation;
	V3F prevPos;
	V3F prevRotation;
	V3F inertialVel;
	V3F inertialRotVel;
	F32 mass;
	M3x3FSymmetric inertiaTensor;
};

ArenaArrayList<Point> points;

struct PositionConstraint {
	I32 constrained;
	I32 relative; // Global position if -1
	V3F offset;
	F32 constraintDistance;
	F32 stiffness;
	F32 finiteStiffness;
	F32 lambda;
	F32 previousEnergy;

	void set_previous_energy(V3F pos) {
		previousEnergy = distance(pos, relative == -1 ? offset : points.data[relative].pos) - constraintDistance;
	}
	F32 energy(V3F pos, F32 alpha) {
		F32 energy = distance(pos, relative == -1 ? offset : points.data[relative].pos) - constraintDistance;
		if (finiteStiffness == F32_INF) {
			energy = energy - previousEnergy * alpha;
		}
		return energy;
	}
	V3F gradient(V3F pos) {
		return normalize_safe(pos - (relative == -1 ? offset : points.data[relative].pos), 0.00001F);
	}
	M3x3FSymmetric hessian(V3F pos) {
		M3x3FSymmetric h;
		V3F rel = pos - (relative == -1 ? offset : points.data[relative].pos);
		F32 dSq = length_sq(rel);
		if (dSq < 0.00001F) {
			return M3x3FSymmetric{};
		}
		F32 dInv = 1.0F / sqrtf32(dSq);
		F32 dInvCu = dInv * dInv * dInv;
		h.m00 = dInv - rel.x * rel.x * dInvCu;
		h.m01 = -rel.x * rel.y * dInvCu;
		h.m02 = -rel.x * rel.z * dInvCu;
		h.m11 = dInv - rel.y * rel.y * dInvCu;
		h.m12 = -rel.y * rel.z * dInvCu;
		h.m22 = dInv - rel.z * rel.z * dInvCu;
		return h;
	}
};

ArenaArrayList<PositionConstraint> constraints;
ArenaArrayList<U32> constraintSortedIndices;
ArenaArrayList<U32> coloredPoints[32];
F64 simTiming;
F64 simTimingSmooth;
U32 threadCount;

I32 add_point(V3F pos, F32 mass) {
	Point& point = points.push_back_zeroed();
	point.pos = pos;
	point.mass = mass;
	return I32(points.size) - 1;
}

void hard_constrain_points(I32 idxA, I32 idxB, F32 dist) {
	PositionConstraint& cA = constraints.push_back_zeroed();
	cA.constrained = idxA;
	cA.relative = idxB;
	cA.constraintDistance = dist;
	cA.finiteStiffness = F32_INF;
	PositionConstraint& cB = constraints.push_back_zeroed();
	cB.constrained = idxB;
	cB.relative = idxA;
	cB.constraintDistance = dist;
	cB.finiteStiffness = F32_INF;
	points.data[idxA].constraintCount++;
	points.data[idxB].constraintCount++;
}
void hard_constrain_point_to(I32 idxA, I32 idxB, F32 dist) {
	PositionConstraint& cA = constraints.push_back_zeroed();
	cA.constrained = idxA;
	cA.relative = idxB;
	cA.constraintDistance = dist;
	cA.finiteStiffness = F32_INF;
	points.data[idxA].constraintCount++;
}
void hard_constrain_point_global(I32 idx, V3F pos, F32 dist) {
	PositionConstraint& c = constraints.push_back_zeroed();
	c.constrained = idx;
	c.relative = -1;
	c.constraintDistance = dist;
	c.offset = pos;
	c.finiteStiffness = F32_INF;
	points.data[idx].constraintCount++;
}

const U32 MAX_PHYSICS_THREADS = 32;
ArenaArrayList<HANDLE> workerThreads;
struct WorkItem {
	U32 color;
	U32 startIdx;
	U32 endIdx;
};
ArenaArrayList<WorkItem> work;
F32 dtSqWorker;
F32 alphaWorker;
F32 lambdaMinWorker;
F32 lambdaMaxWorker;
I32 completedWorkItems;
RWSpinLock workLock;
bool workerShouldDie;

DWORD __stdcall physics_worker(LPVOID param) {
	while (!workerShouldDie) {
		WorkItem item{};
		bool popped = false;
		workLock.lock_write();
		if (!work.empty()) {
			item = work.pop_back();
			popped = true;
		}
		workLock.unlock_write();
		if (!popped) {
			continue;
		}
		for (U32 colorIdx = item.startIdx; colorIdx < item.endIdx; colorIdx++) {
			U32 pointIdx = coloredPoints[item.color].data[colorIdx];
			Point& p = points.data[pointIdx];
			//V3F force = (p.inertialPos - p.pos) * p.mass / dtSq;
			V3F force = p.inertialVel * p.mass / dtSqWorker;
			M3x3FSymmetric hessian{};
			hessian.m00 = hessian.m11 = hessian.m22 = p.mass / dtSqWorker;
			for (U32 constraintIdx = pointIdx == 0 ? 0 : points.data[pointIdx - 1].constraintEndOffset; constraintIdx < p.constraintEndOffset; constraintIdx++) {
				PositionConstraint& c = constraints[constraintSortedIndices[constraintIdx]];
				V3F grad = c.gradient(p.pos);
				F32 energy = c.energy(p.pos, alphaWorker);
				bool hardConstraint = c.finiteStiffness == F32_INF;
				if (hardConstraint) {
					force -= clamp(c.stiffness * energy + c.lambda, lambdaMinWorker, lambdaMaxWorker) * grad;
				} else {
					force -= c.stiffness * energy * grad;
				}
				F32 lambdaPlus = c.stiffness * energy + c.lambda;
				hessian += c.stiffness * M3x3FSymmetric{ grad.x * grad.x, grad.x * grad.y, grad.x * grad.z,
					grad.y * grad.y, grad.y * grad.z,
					grad.z * grad.z };
				M3x3FSymmetric g = lambdaPlus * c.hessian(p.pos);
				hessian.m00 += sqrtf32(g.m00 * g.m00 + g.m01 * g.m01 + g.m02 * g.m02);
				hessian.m11 += sqrtf32(g.m01 * g.m01 + g.m11 * g.m11 + g.m12 * g.m12);
				hessian.m22 += sqrtf32(g.m02 * g.m02 + g.m12 * g.m12 + g.m22 * g.m22);
			}
			V3F posDelta = hessian.solve_ldlt(force);
			p.pos += posDelta;
			p.inertialVel -= posDelta;
		}
		_ReadWriteBarrier();
		_InterlockedIncrement((unsigned int*)&completedWorkItems);
	}
	ExitThread(0);
}

void init_threads(U32 threads) {
	if (!workerThreads.empty()) {
		workerShouldDie = true;
		_ReadWriteBarrier();
		WaitForMultipleObjects(workerThreads.size, workerThreads.data, TRUE, INFINITE);
		workerShouldDie = false;
		workerThreads.clear();
	}
	for (U32 i = 0; i < threads; i++) {
		HANDLE thread = CreateThread(NULL, 1024 * 256, physics_worker, NULL, 0, NULL);
		workerThreads.push_back(thread);
	}

}

void do_timestep_parallel(F32 dt, U32 iterations, F32 alpha, F32 beta, F32 gamma, U32 threads) {
	threads = clamp(threads, 1u, MAX_PHYSICS_THREADS);
	if (threads != threadCount) {
		init_threads(threads);
	}
	threadCount = threads;
	F64 startTime = current_time_seconds();

	F32 lambdaMin = 0.0F;
	F32 lambdaMax = 1000000.0F;
	F32 stiffnessStart = 1.0F;
	F32 stiffnessMax = 1000000.0F;
	
	F64 dt64 = F64(dt);
	F32 dtSq = F32(dt64 * dt64);

	V3F acceleration = V3F{ 0.0F, -9.81F, 0.0F };
	U32 constraintTotal = 0;
	for (U32 i = 0; i < points.size; i++) {
		Point& p = points.data[i];
		p.prevPos = p.pos;
		p.inertialVel = dt * p.vel + dtSq * acceleration;
		//p.inertialPos = p.pos + dt * p.vel + dtSq * acceleration;
		//p.pos = p.pos + dt * p.vel + dtSq * acceleration;
		p.constraintEndOffset = constraintTotal;
		constraintTotal += p.constraintCount;
		p.graphColor = 0;
	}
	constraintSortedIndices.resize(constraints.size);
	for (U32 i = 0; i < constraints.size; i++) {
		PositionConstraint& c = constraints.data[i];
		Point& p = points.data[c.constrained];
		constraintSortedIndices.data[p.constraintEndOffset++] = i;
		c.stiffness = max(gamma * c.stiffness, stiffnessStart);
		c.lambda = alpha * gamma * c.lambda;
		c.set_previous_energy(p.pos);
	}
	for (U32 i = 0; i < ARRAY_COUNT(coloredPoints); i++) {
		coloredPoints[i].clear();
	}

	U32 maxGraphColor = 0;
	{ // Graph color (greedy, might be worth looking into better approximations)
		U32 constraintIdx = 0;
		for (U32 i = 0; i < points.size; i++) {
			Point& p = points.data[i];
			U32 availableColors = 0xFFFFFFFF;
			for (; constraintIdx < p.constraintEndOffset; constraintIdx++) {
				PositionConstraint& c = constraints.data[constraintSortedIndices.data[constraintIdx]];
				if (c.relative != -1) {
					availableColors &= ~points.data[c.relative].graphColor;
				}
			}
			availableColors |= 1u << 31;
			p.graphColor = 1u << tzcnt32(availableColors);
			coloredPoints[tzcnt32(p.graphColor)].push_back(i);
			maxGraphColor = max(maxGraphColor, p.graphColor);
		}
	}
	while (iterations--) {
		dtSqWorker = dtSq;
		alphaWorker = alpha;
		lambdaMinWorker = lambdaMin;
		lambdaMaxWorker = lambdaMax;
		for (U32 currentColor = 0; currentColor <= tzcnt32(maxGraphColor); currentColor++) {
			U32 pointsToProcess = coloredPoints[currentColor].size;
			U32 pointsPerThread = pointsToProcess / threadCount;
			workLock.lock_write();
			completedWorkItems = 0;
			for (U32 t = 0; t < threadCount; t++) {
				work.push_back(WorkItem{ currentColor, pointsPerThread * t, t == threadCount - 1 ? pointsToProcess : pointsPerThread * (t + 1) });
			}
			workLock.unlock_write();
			while (U32(__iso_volatile_load32((int*)&completedWorkItems)) != threadCount);
			workLock.lock_write();
			work.clear();
			workLock.unlock_write();
		}
		for (U32 constraintIdx = 0; constraintIdx < constraints.size; constraintIdx++) {
			PositionConstraint& c = constraints.data[constraintIdx];
			F32 energy = c.energy(points.data[c.constrained].pos, alpha);
			bool hardConstraint = c.finiteStiffness == F32_INF;
			if (hardConstraint) {
				c.lambda = clamp(c.stiffness * energy + c.lambda, lambdaMin, lambdaMax);
				if (c.lambda > lambdaMin && c.lambda < lambdaMax) {
					c.stiffness = min(stiffnessMax, c.stiffness + beta * absf32(energy));
				}
			} else {
				c.stiffness = min(stiffnessMax, c.finiteStiffness, c.stiffness + beta * absf32(energy));
			}
		}
	}
	for (Point& p : points) {
		p.vel = (p.pos - p.prevPos) / dt;
	}

	F64 endTime = current_time_seconds();
	simTiming = roundf64((endTime - startTime) * 10000.0);
	simTimingSmooth = simTiming * 0.1 + simTimingSmooth * 0.9;
}

void do_timestep(F32 dt, U32 iterations, F32 alpha, F32 beta, F32 gamma, U32 threads) {
	F64 startTime = current_time_seconds();

	F32 lambdaMin = 0.0F;
	F32 lambdaMax = 1000000.0F;
	F32 stiffnessStart = 1.0F;
	F32 stiffnessMax = 1000000.0F;

	F64 dt64 = F64(dt);
	F32 dtSq = F32(dt64 * dt64);

	V3F acceleration = V3F{ 0.0F, -9.81F, 0.0F };
	U32 constraintTotal = 0;
	for (U32 i = 0; i < points.size; i++) {
		Point& p = points.data[i];
		p.prevPos = p.pos;
		p.inertialVel = dt * p.vel + dtSq * acceleration;
		//p.inertialPos = p.pos + dt * p.vel + dtSq * acceleration;
		p.constraintEndOffset = constraintTotal;
		constraintTotal += p.constraintCount;
		p.graphColor = 0;
	}
	constraintSortedIndices.resize(constraints.size);
	for (U32 i = 0; i < constraints.size; i++) {
		PositionConstraint& c = constraints.data[i];
		Point& p = points.data[c.constrained];
		constraintSortedIndices.data[p.constraintEndOffset++] = i;
		c.stiffness = max(gamma * c.stiffness, stiffnessStart);
		c.lambda = alpha * gamma * c.lambda;
		c.set_previous_energy(p.pos);
	}

	U32 maxGraphColor = 0;
	{ // Graph color (greedy, might be worth looking into better approximations)
		U32 constraintIdx = 0;
		for (U32 i = 0; i < points.size; i++) {
			Point& p = points.data[i];
			U32 availableColors = 0xFFFFFFFF;
			for (; constraintIdx < p.constraintEndOffset; constraintIdx++) {
				PositionConstraint& c = constraints.data[constraintSortedIndices.data[constraintIdx]];
				if (c.relative) {
					availableColors &= ~points.data[c.relative].graphColor;
				}
			}
			availableColors |= 1u << 31;
			p.graphColor = 1u << tzcnt32(availableColors);
			maxGraphColor = max(maxGraphColor, p.graphColor);
		}
	}
	while (iterations--) {
		for (U32 currentColor = 1; currentColor && currentColor <= maxGraphColor; currentColor <<= 1) {
			for (U32 pointIdx = 0; pointIdx < points.size; pointIdx++) {
				Point& p = points.data[pointIdx];
				if (p.graphColor != currentColor) {
					continue;
				}
				//V3F force = (p.inertialPos - p.pos) * p.mass / dtSq;
				V3F force = p.inertialVel * p.mass / dtSq;
				M3x3FSymmetric hessian{};
				hessian.m00 = hessian.m11 = hessian.m22 = p.mass / dtSq;
				for (U32 constraintIdx = pointIdx == 0 ? 0 : points.data[pointIdx - 1].constraintEndOffset; constraintIdx < p.constraintEndOffset; constraintIdx++) {
					PositionConstraint& c = constraints[constraintSortedIndices[constraintIdx]];
					V3F grad = c.gradient(p.pos);
					F32 energy = c.energy(p.pos, alpha);
					bool hardConstraint = c.finiteStiffness == F32_INF;
					if (hardConstraint) {
						force -= clamp(c.stiffness * energy + c.lambda, lambdaMin, lambdaMax) * grad;
					} else {
						force -= c.stiffness * energy * grad;
					}
					F32 lambdaPlus = c.stiffness * energy + c.lambda;
					hessian += c.stiffness * M3x3FSymmetric{ grad.x * grad.x, grad.x * grad.y, grad.x * grad.z,
						grad.y * grad.y, grad.y * grad.z,
						grad.z * grad.z };
					M3x3FSymmetric g = lambdaPlus * c.hessian(p.pos);
					hessian.m00 += sqrtf32(g.m00 * g.m00 + g.m01 * g.m01 + g.m02 * g.m02);
					hessian.m11 += sqrtf32(g.m01 * g.m01 + g.m11 * g.m11 + g.m12 * g.m12);
					hessian.m22 += sqrtf32(g.m02 * g.m02 + g.m12 * g.m12 + g.m22 * g.m22);
				}
				V3F posDelta = hessian.solve_ldlt(force);
				p.pos += posDelta;
				p.inertialVel -= posDelta;
			}
		}
		for (U32 constraintIdx = 0; constraintIdx < constraints.size; constraintIdx++) {
			PositionConstraint& c = constraints.data[constraintIdx];
			F32 energy = c.energy(points.data[c.constrained].pos, alpha);
			bool hardConstraint = c.finiteStiffness == F32_INF;
			if (hardConstraint) {
				c.lambda = clamp(c.stiffness * energy + c.lambda, lambdaMin, lambdaMax);
				if (c.lambda > lambdaMin && c.lambda < lambdaMax) {
					c.stiffness = min(stiffnessMax, c.stiffness + beta * absf32(energy));
				}
			} else {
				c.stiffness = min(stiffnessMax, c.finiteStiffness, c.stiffness + beta * absf32(energy));
			}
		}
	}
	for (Point& p : points) {
		p.vel = (p.pos - p.prevPos) / dt;
	}

	F64 endTime = current_time_seconds();
	simTiming = roundf64((endTime - startTime) * 10000.0);
	simTimingSmooth = simTiming * 0.1 + simTimingSmooth * 0.9;
}



void debug_render() {
	DynamicVertexBuffer::Tessellator& tes = DynamicVertexBuffer::get_tessellator();
	tes.begin_draw(VK::debugLinesPipeline, VK::drawPipelineLayout, DynamicVertexBuffer::DRAW_MODE_PRIMITIVES);
	for (PositionConstraint& c : constraints) {
		V3F a = points.data[c.constrained].pos;
		V3F b = c.relative == -1 ? c.offset : points.data[c.relative].pos;
		tes.pos3(a.x, a.y, a.z).color(1.0F, 0.0F, 0.0F).end_vert();
		tes.pos3(b.x, b.y, b.z).color(1.0F, 0.0F, 0.0F).end_vert();
	}
	tes.end_draw();

	tes.begin_draw(VK::debugPointsPipeline, VK::drawPipelineLayout, DynamicVertexBuffer::DRAW_MODE_PRIMITIVES);
	for (Point& p : points) {
		tes.pos3(p.pos.x, p.pos.y, p.pos.z).color(1.0F, 0.0F, 0.0F).end_vert();
	}
	tes.end_draw();
}

}