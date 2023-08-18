#pragma once
#include <intrin.h>
#include <immintrin.h>
#include "DrillLibDefs.h"

// The cos approximation is more accurate than the sin one I generated (gentler curve over [0, 0.5) I guess), so we can just use it for sin as well
// Cos approximation: 1.00010812282562255859375 + x * (-1.79444365203380584716796875e-2 + x * (-19.2416629791259765625 + x * (-5.366222381591796875 + x * (93.06533050537109375 + x * (-74.45227813720703125)))))
// Found using Sollya (https://www.sollya.org/)
// This isn't a particularly good polynomial approximation in general, but it's good enough for me.
// It's a good bit faster than the microsoft standard library implementation while trading off some accuracy, which is a win for me.
// This paper should provide some info on how to make it better if I need it (https://arxiv.org/pdf/1508.03211.pdf)

// TODO AVX2 (x8) implementations

FINLINE __m128 cosfx4(__m128 xmmX) {
	__m128 xRoundedDown = _mm_round_ps(xmmX, _MM_ROUND_MODE_DOWN);
	__m128 xIn0to1 = _mm_sub_ps(xmmX, xRoundedDown);
	__m128 point5 = _mm_set_ps1(0.5F);
	__m128 xIn0to1MinusPoint5 = _mm_sub_ps(xIn0to1, point5);
	__m128 isGreaterThanPoint5 = _mm_cmpge_ps(xIn0to1, point5);
	__m128 xInRange0ToPoint5 = _mm_blendv_ps(xIn0to1, xIn0to1MinusPoint5, isGreaterThanPoint5);
	__m128 sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, _mm_set_ps1(-74.45227813720703125F), _mm_set_ps1(93.06533050537109375F));
	sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm_set_ps1(-5.366222381591796875F));
	sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm_set_ps1(-19.2416629791259765625F));
	sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm_set_ps1(-1.79444365203380584716796875e-2F));
	sinApprox = _mm_fmadd_ps(xInRange0ToPoint5, sinApprox, _mm_set_ps1(1.00010812282562255859375F));
	__m128 sinApproxFlipped = _mm_sub_ps(_mm_setzero_ps(), sinApprox);
	sinApprox = _mm_blendv_ps(sinApprox, sinApproxFlipped, isGreaterThanPoint5);
	return sinApprox;
}

FINLINE f32 cosf(f32 x) {
	// MSVC is very bad at optimizing *_ss intrinsics, so it's actually faster to simply use the x4 version and take one of the results
	// The *_ss version was more than 50% (!) slower in my tests, and a quick look in godbolt explains why
	return _mm_cvtss_f32(cosfx4(_mm_set_ps1(x)));
	/*__m128 xmmX = _mm_set_ss(x);
	__m128 xRoundedDown = _mm_round_ss(_mm_undefined_ps(), xmmX, _MM_ROUND_MODE_DOWN);
	__m128 xIn0to1 = _mm_sub_ss(xmmX, xRoundedDown);
	__m128 point5 = _mm_set_ss(0.5F);
	__m128 xIn0to1MinusPoint5 = _mm_sub_ss(xIn0to1, point5);
	__m128 isGreaterThanPoint5 = _mm_cmpge_ss(xIn0to1, point5);
	__m128 xInRange0ToPoint5 = _mm_blendv_ps(xIn0to1, xIn0to1MinusPoint5, isGreaterThanPoint5);
	__m128 sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, _mm_set_ss(-74.45227813720703125F), _mm_set_ss(93.06533050537109375F));
	sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, sinApprox, _mm_set_ss(-5.366222381591796875F));
	sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, sinApprox, _mm_set_ss(-19.2416629791259765625F));
	sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, sinApprox, _mm_set_ss(-1.79444365203380584716796875e-2F));
	sinApprox = _mm_fmadd_ss(xInRange0ToPoint5, sinApprox, _mm_set_ss(1.00010812282562255859375F));
	__m128 sinApproxFlipped = _mm_sub_ss(_mm_setzero_ps(), sinApprox);
	sinApprox = _mm_blendv_ps(sinApprox, sinApproxFlipped, isGreaterThanPoint5);
	return _mm_cvtss_f32(sinApprox);*/
}


FINLINE __m128 sinfx4(__m128 xmmX) {
	return cosfx4(_mm_sub_ps(xmmX, _mm_set_ps1(0.25F)));
}

FINLINE f32 sinf(f32 x) {
	// MSVC is very bad at optimizing *_ss intrinsics, so it's actually faster to simply use the x4 version and take one of the results
	return _mm_cvtss_f32(cosfx4(_mm_set_ps1(x - 0.25F)));
}

FINLINE f32 sqrtf(f32 x) {
	return _mm_cvtss_f32(_mm_sqrt_ps(_mm_set_ss(x)));
}

FINLINE f32 sincosf(f32* sinOut, f32 x) {
	float cosine = cosf(x);
	*sinOut = sinf(x); // Could probably optimize this using the pythagorean identity and the result of cosf, I'll figure it out later
	return cosine;
}

FINLINE f32 fractf(f32 f) {
	return f - _mm_cvtss_f32(_mm_round_ps(_mm_set_ss(f), _MM_ROUND_MODE_TOWARD_ZERO));
}

FINLINE f32 truncf(f32 f) {
	return _mm_cvtss_f32(_mm_round_ps(_mm_set_ss(f), _MM_ROUND_MODE_TOWARD_ZERO));
}

#pragma pack(push, 1)
struct Vector2f {
	float x, y;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Vector3f {
	f32 x, y, z;

	FINLINE f32 length_sq() {
		return x * x + y * y + z * z;
	}

	FINLINE f32 length() {
		return sqrtf(x * x + y * y + z * z);
	}
};
#pragma pack(pop)

FINLINE Vector3f operator+(Vector3f v, f32 f) {
	return Vector3f{ v.x + f, v.y + f, v.z + f };
}

FINLINE Vector3f operator+(f32 f, Vector3f v) {
	return Vector3f{ v.x + f, v.y + f, v.z + f };
}

FINLINE Vector3f operator+(Vector3f a, Vector3f b) {
	return Vector3f{ a.x + b.x, a.y + b.y, a.z + b.z };
}

FINLINE Vector3f operator-(Vector3f v, f32 f) {
	return Vector3f{ v.x - f, v.y - f, v.z - f };
}

FINLINE Vector3f operator-(Vector3f v) {
	return Vector3f{ -v.x, -v.y, -v.z };
}

FINLINE Vector3f operator*(Vector3f v, f32 f) {
	return Vector3f{ v.x * f, v.y * f, v.z * f };
}

FINLINE Vector3f operator*(f32 f, Vector3f v) {
	return Vector3f{ v.x * f, v.y * f, v.z * f };
}

FINLINE Vector3f operator/(Vector3f v, f32 f) {
	f32 invF = 1.0F / f;
	return Vector3f{ v.x * invF, v.y * invF, v.z * invF };
}

FINLINE Vector3f cross(Vector3f a, Vector3f b) {
	return Vector3f{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

FINLINE f32 length_sq(Vector3f v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

FINLINE f32 length(Vector3f v) {
	return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}



void println_vec3f(Vector3f vec) {
	print("(");
	print_float(vec.x);
	print(", ");
	print_float(vec.y);
	print(", ");
	print_float(vec.z);
	print(")\n");
}

#pragma pack(push, 1)
struct Vector4f {
	f32 x, y, z, w;
};
#pragma pack(pop)

struct Quaternionf {
	f32 x, y, z, w;

	// https://www.3dgep.com/understanding-quaternions/
	// https://fgiesen.wordpress.com/2019/02/09/rotating-a-single-vector-using-a-quaternion/
	FINLINE Vector3f transform(Vector3f v) {
		f32 x2 = x + x;
		f32 y2 = y + y;
		f32 z2 = z + z;
		f32 tx = (y2 * v.z - z2 * v.y);
		f32 ty = (z2 * v.x - x2 * v.z);
		f32 tz = (x2 * v.y - y2 * v.x);
		return Vector3f {
			v.x + w * tx + (y * tz - z * ty),
			v.y + w * ty + (z * tx - x * tz),
			v.z + w * tz + (x * ty - y * tx)
		};
	}

	FINLINE Quaternionf conjugate() {
		return Quaternionf{ -x, -y, -z, w };
	}

	FINLINE f32 magnitude_sq() {
		return x * x + y * y + z * z + w * w;
	}

	FINLINE f32 magnitude() {
		return sqrtf(x * x + y * y + z * z + w * w);
	}

	Quaternionf normalize() {
		f32 invMagnitude = 1.0F / magnitude();
		return Quaternionf{ x * invMagnitude, y * invMagnitude, z * invMagnitude, w * invMagnitude };
	}

	Quaternionf inverse() {
		f32 invMagSq = 1.0F / magnitude_sq();
		return Quaternionf{ -x * invMagSq, -y * invMagSq, -z * invMagSq, w * invMagSq };
	}
};

FINLINE Quaternionf operator*(Quaternionf a, Quaternionf b) {
	// General equation for a quaternion product
	// q.w = a.w * b.w - dot(a.v, b.v)
	// q.v = a.w * b.v + b.w * a.v + cross(a.v, b.v);
	// This can be derived by using the form (xi + yj + zk + w) and multiplying two of them together
	return Quaternionf{
		a.w * b.x + b.w * a.x + (a.y * b.z - a.z * b.y),
		a.w * b.y + b.w * a.y + (a.z * b.x - a.x * b.z),
		a.w * b.z + b.w * a.y + (a.x * b.y - a.y * b.x),
		a.w * b.w - (a.x * b.x + a.y * b.y + a.z * b.z)
	};
}

struct AxisAnglef {
	Vector3f axis;
	f32 angle;

	FINLINE Quaternionf to_quaternion() {
		// A quaternion is { v * sin(theta/2), cos(theta/2) }, where v is the axis and theta is the angle
		f32 cosAngleOver2 = cosf(angle * 0.5F);
		f32 sinAngleOver2 = sinf(angle * 0.5F);
		return Quaternionf{ axis.x * sinAngleOver2, axis.x * sinAngleOver2, axis.x * sinAngleOver2, cosAngleOver2 };

	}
};

// I don't think this counts as a 4x3 matrix, it's really a 4x4 matrix with the 4th row assumed to be 0, 0, 0, 1
// I should come up with a better name for this
#pragma pack(push, 1)
struct Matrix4x3f {
	f32 m00, m01, m02, x,
		m10, m11, m12, y,
		m20, m21, m22, z;

	FINLINE Matrix4x3f& set_zero() {
		m00 = 0.0F;
		m01 = 0.0F;
		m02 = 0.0F;
		m10 = 0.0F;
		m11 = 0.0F;
		m12 = 0.0F;
		m20 = 0.0F;
		m21 = 0.0F;
		m22 = 0.0F;
		x = 0.0F;
		y = 0.0F;
		z = 0.0F;
		return *this;
	}

	FINLINE Matrix4x3f& set_identity() {
		m00 = 1.0F;
		m01 = 0.0F;
		m02 = 0.0F;
		m10 = 0.0F;
		m11 = 1.0F;
		m12 = 0.0F;
		m20 = 0.0F;
		m21 = 0.0F;
		m22 = 1.0F;
		x = 0.0F;
		y = 0.0F;
		z = 0.0F;
		return *this;
	}

	FINLINE Matrix4x3f transpose_rotation() {
		return Matrix4x3f{
			m00, m10, m20, x,
			m01, m11, m21, y,
			m02, m12, m22, z
		};
	}

	// This is a simplification of Quaternionf.rotate(), substituting in (1, 0, 0), (0, 1, 0), and (0, 0, 1) as the vectors to rotate
	// Pretty much we just rotate the basis vectors of the identity matrix
	FINLINE Matrix4x3f& set_rotation_from_quat(Quaternionf q) {
		f32 yy2 = 2.0F * q.y * q.y;
		f32 zz2 = 2.0F * q.z * q.z;
		f32 zw2 = 2.0F * q.z * q.w;
		f32 xy2 = 2.0F * q.x * q.y;
		f32 xz2 = 2.0F * q.x * q.z;
		f32 yw2 = 2.0F * q.y * q.w;
		f32 xx2 = 2.0F * q.x * q.x;
		f32 xw2 = 2.0F * q.x * q.w;
		f32 yz2 = 2.0F * q.y * q.z;
		m00 = 1.0F - yy2 - zz2;
		m10 = zw2 + xy2;
		m20 = xz2 - yw2;
		m01 = xy2 - zw2;
		m11 = 1.0F - zz2 - xx2;
		m21 = xw2 + yz2;
		m02 = yw2 + xz2;
		m12 = yz2 - xw2;
		m22 = 1.0F - xx2 - yy2;
		return *this;
	}

	FINLINE Matrix4x3f& set_offset(Vector3f offset) {
		x = offset.x;
		y = offset.y;
		z = offset.z;
		return *this;
	}

	FINLINE Matrix4x3f& translate(Vector3f offset) {
		x += m00 * offset.x + m01 * offset.y + m02 * offset.z;
		y += m10 * offset.x + m11 * offset.y + m12 * offset.z;
		z += m20 * offset.x + m21 * offset.y + m22 * offset.z;
		return *this;
	}

	FINLINE Matrix4x3f& rotate(Quaternionf q) {
		Vector3f row0 = q.transform(Vector3f{ m00, m01, m02 });
		Vector3f row1 = q.transform(Vector3f{ m10, m11, m12 });
		Vector3f row2 = q.transform(Vector3f{ m20, m21, m22 });
		Vector3f offset = q.transform(Vector3f{ x, y, z });
		m00 = row0.x;
		m01 = row0.y;
		m02 = row0.z;
		x = offset.x;
		m10 = row1.x;
		m11 = row1.y;
		m12 = row1.z;
		y = offset.y;
		m20 = row2.x;
		m21 = row2.y;
		m22 = row2.z;
		z = offset.z;
		return *this;
	}

	FINLINE Matrix4x3f& rotate_local(Quaternionf q) {
		Vector3f row0 = q.transform(Vector3f{ m00, m01, m02 });
		Vector3f row1 = q.transform(Vector3f{ m10, m11, m12 });
		Vector3f row2 = q.transform(Vector3f{ m20, m21, m22 });
		m00 = row0.x;
		m01 = row0.y;
		m02 = row0.z;
		m10 = row1.x;
		m11 = row1.y;
		m12 = row1.z;
		m20 = row2.x;
		m21 = row2.y;
		m22 = row2.z;
		return *this;
	}

	FINLINE Matrix4x3f& scale(Vector3f scale) {
		m00 *= scale.x;
		m01 *= scale.x;
		m02 *= scale.x;
		x *= scale.x;
		m10 *= scale.y;
		m11 *= scale.y;
		m12 *= scale.y;
		y *= scale.y;
		m20 *= scale.z;
		m21 *= scale.z;
		m22 *= scale.z;
		z *= scale.z;
		return *this;
	}

	FINLINE Vector3f transform(Vector3f vec) {
		return Vector3f{
			x + m00 * vec.x + m01 * vec.y + m02 * vec.z,
			y + m10 * vec.x + m11 * vec.y + m12 * vec.z,
			z + m20 * vec.x + m21 * vec.y + m22 * vec.z
		};
	}

	FINLINE Vector3f get_row(u32 idx) {
		if (idx == 0) {
			return Vector3f{ m00, m01, m02 };
		} else if (idx == 1) {
			return Vector3f{ m10, m11, m12 };
		} else {
			return Vector3f{ m20, m21, m22 };
		}
	}

	FINLINE Matrix4x3f& set_row(u32 idx, Vector3f row) {
		if (idx == 0) {
			m00 = row.x;
			m01 = row.y;
			m02 = row.z;
		} else if (idx == 1) {
			m10 = row.x;
			m11 = row.y;
			m12 = row.z;
		} else {
			m20 = row.x;
			m21 = row.y;
			m22 = row.z;
		}
		return *this;
	}
};
#pragma pack(pop)

Matrix4x3f operator*(const Matrix4x3f& a, const Matrix4x3f& b) {
	Matrix4x3f dst;
	dst.m00 = a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20;
	dst.m01 = a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21;
	dst.m02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22;
	dst.x   = a.m00 * b.x   + a.m01 * b.y   + a.m02 * b.z   + a.x;
	dst.m10 = a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20;
	dst.m11 = a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21;
	dst.m12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22;
	dst.y   = a.m10 * b.x   + a.m11 * b.y   + a.m12 * b.z   + a.y;
	dst.m20 = a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20;
	dst.m21 = a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21;
	dst.m22 = a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22;
	dst.z   = a.m20 * b.x   + a.m21 * b.y   + a.m22 * b.z   + a.z;
	return dst;
}

struct PerspectiveProjection {
	f32 xScale;
	f32 xZBias;
	f32 yScale;
	f32 yZBias;
	f32 nearPlane;

	// I decided to think about this intuitively for once instead of just copying some standard projection matrix diagram
	// This is my thought process in case I have to debug it later
	//
	// So we start the fov in 4 directions and a nearplane
	// For each fov direction, we can use sin and cosine to get the direction vector from the angle, then project that onto a plane at z=1
	// Getting the coordinate that isn't 1 is all that matters here, so we can scale the x/y component by the inverse of the one pointing in the z direction
	// This results in sin over cos telling us where the projected vectors extend to on the z=1 plane
	// All four of these numbers defines a rectangle on the z=1 plane
	// r and l will be the right and left x coords of the rectangle and u and d will be the up and down y coords of the rectangle
	// Now, to project, we want to project everything onto the rectangle through the view point, scaling everything down with distance linearly
	// I'll consider only the x coordinate first, since the y should be similar
	// We want to scale to the center of the rectangle, so first off, we have to translate the space so the center of the rectangle is at x=0
	// xCenter = (r + l) / 2
	// x = x - xCenter
	// This is a linear translation, but to transform the frustum space correctly, it needs to be perspective. We can simply scale up the translation value with distance from camera
	// x = x - z * xCenter
	// Now, things at Z = 1 should be centered on x = 0
	// To get the l to r range into the -1 to 1 NDC range, we can divide out half the distance from l to r, scaling the rectangle to a -1 to 1 square
	// halfXRange = (r - l) / 2
	// x = (x - z * xCenter) / halfXRange
	// This works for a skewed orthographic projection, but since we want perspective, we still need to scale stuff down
	// Something twice as far away should scale down twice as much, so we can divide by z to scale things linearly with distance
	// x = (x - z * xCenter) / (z * halfXRange)
	// x = (x - z * (r + l) * 0.5) / (z * (r - l) * 0.5)
	// The y axis should be quite similar
	// y = (y - z * (u + d) * 0.5) / (z * (u - d) * 0.5)
	// Rearranging so we get easy constants and divide by z at the end so we can use that as a w coordinate in a shader
	// x = ((x - z * (r + l) * 0.5) / ((r - l) * 0.5)) / z
	// x = ((x / ((r - l) * 0.5)) - (z * (r + l) * 0.5) / ((r - l) * 0.5)) / z
	// x = (x * (1 / ((r - l) * 0.5)) - z * ((r + l) / (r - l))) / z
	// y = (y * (1 / ((u - d) * 0.5)) - z * ((u + d) / (u - d))) / z
	// Since we're using reverse z, our resulting depth should decrease as distance increases, in order to get better resolution at distance
	// z = 1 / z
	// This puts things less than z=1 at a depth greater than 1 (out of bounds), but we'd like things less than the near plane to be out of bounds, so we can scale the depth down by the near plane
	// z = nearPlane / z
	// Alright, that's all our values, with a division by z at the end, so we can just stick that in the w coordinate in the shader so the division happens automatically
	// This gives us 5 constants to pass
	// xScale = 1 / ((r - l) * 0.5)
	// xZBias = (r + l) / (r - l)
	// yScale = 1 / ((u - d) * 0.5)
	// yZBias = (u + d) / (u - d)
	// nearPlane = nearPlane
	FINLINE void project_perspective(float nearZ, float fovRight, float fovLeft, float fovUp, float fovDown) {
		f32 sinRight;
		f32 cosRight = sincosf(&sinRight, fovRight);
		f32 rightX = sinRight / cosRight;
		f32 sinLeft;
		f32 cosLeft = sincosf(&sinLeft, fovLeft);
		f32 leftX = sinLeft / cosLeft;
		f32 sinUp;
		f32 cosUp = sincosf(&sinUp, fovUp);
		f32 upY = sinUp / cosUp;
		f32 sinDown;
		f32 cosDown = sincosf(&sinDown, fovDown);
		f32 downY = sinDown / cosDown;
		xScale = 1.0F / ((rightX - leftX) * 0.5F);
		xZBias = (rightX + leftX) / (rightX - leftX);
		yScale = 1.0F / ((upY - downY) * 0.5F);
		yZBias = (upY + downY) / (upY - downY);
		nearPlane = nearZ;
	}

	FINLINE Vector3f transform(Vector3f vec) {
		f32 x = vec.x * xScale + vec.z * xZBias;
		f32 y = vec.y * yScale + vec.z * yZBias;
		f32 z = nearPlane;
		f32 invZ = -1.0F / vec.z;
		return Vector3f{ x * invZ, y * invZ, z * invZ };
	}
};

// Projective matrices only output the near plane to the z component, so we can optimize away the third row of the 4x4 matrix
struct ProjectiveTransformMatrix {
	f32 m00, m01, m02, m03,
		m10, m11, m12, m13,
		//m20, m21, m22, m23,
		m30, m31, m32, m33;

	FINLINE void generate(PerspectiveProjection projection, Matrix4x3f transform) {
		m00 = transform.m00 * projection.xScale + transform.m20 * projection.xZBias;
		m01 = transform.m01 * projection.xScale + transform.m21 * projection.xZBias;
		m02 = transform.m02 * projection.xScale + transform.m22 * projection.xZBias;
		m03 = transform.x * projection.xScale + transform.z * projection.xZBias;
		m10 = transform.m10 * projection.yScale + transform.m20 * projection.yZBias;
		m11 = transform.m11 * projection.yScale + transform.m21 * projection.yZBias;
		m12 = transform.m12 * projection.yScale + transform.m22 * projection.yZBias;
		m13 = transform.y * projection.yScale + transform.z * projection.yZBias;
		m30 = -transform.m20;
		m31 = -transform.m21;
		m32 = -transform.m22;
		m33 = -transform.z;
	}
};