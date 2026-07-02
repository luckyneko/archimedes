/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

// Minimal column-major (GLSL-compatible) matrix/vector math for the testbed demo.
// Just enough for a perspective camera + a spinning model — deliberately not a math
// library (the renderer treats transforms as opaque bytes via uniform buffers).
// Conventions match GLSL/Vulkan: column-major storage, right-handed, [0,1] depth, and
// a Y-flip baked into the projection so +Y is up on screen.

#include <cmath>

namespace tb
{
	struct Vec3
	{
		float x{0}, y{0}, z{0};
	};

	inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
	inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline Vec3 cross(const Vec3& a, const Vec3& b)
	{
		return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	}
	inline Vec3 normalize(const Vec3& v)
	{
		const float len = std::sqrt(dot(v, v));
		return len > 0.0f ? Vec3{v.x / len, v.y / len, v.z / len} : v;
	}

	// 4x4 matrix, column-major: m[col * 4 + row].
	struct Mat4
	{
		float m[16]{};
	};

	inline Mat4 identity()
	{
		Mat4 r;
		r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
		return r;
	}

	// C = A * B (column-major): C[col][row] = sum_k A[k][row] * B[col][k].
	inline Mat4 operator*(const Mat4& a, const Mat4& b)
	{
		Mat4 c;
		for (int col = 0; col < 4; ++col)
			for (int row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k)
					sum += a.m[k * 4 + row] * b.m[col * 4 + k];
				c.m[col * 4 + row] = sum;
			}
		return c;
	}

	// Right-handed perspective with [0,1] depth and a Y-flip for Vulkan's screen space.
	inline Mat4 perspective(float fovYRadians, float aspect, float nearZ, float farZ)
	{
		const float t = std::tan(fovYRadians * 0.5f);
		Mat4 r;
		r.m[0] = 1.0f / (aspect * t);
		r.m[5] = -1.0f / t; // negative: Vulkan clip-space Y points down
		r.m[10] = farZ / (nearZ - farZ);
		r.m[11] = -1.0f;
		r.m[14] = (nearZ * farZ) / (nearZ - farZ);
		return r;
	}

	// Right-handed look-at: eye position, target point, up direction.
	inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
	{
		const Vec3 f = normalize(center - eye);
		const Vec3 s = normalize(cross(f, up));
		const Vec3 u = cross(s, f);
		Mat4 r = identity();
		r.m[0] = s.x;
		r.m[4] = s.y;
		r.m[8] = s.z;
		r.m[1] = u.x;
		r.m[5] = u.y;
		r.m[9] = u.z;
		r.m[2] = -f.x;
		r.m[6] = -f.y;
		r.m[10] = -f.z;
		r.m[12] = -dot(s, eye);
		r.m[13] = -dot(u, eye);
		r.m[14] = dot(f, eye);
		return r;
	}

	inline Mat4 translate(const Vec3& t)
	{
		Mat4 r = identity();
		r.m[12] = t.x;
		r.m[13] = t.y;
		r.m[14] = t.z;
		return r;
	}

	inline Mat4 rotateY(float a)
	{
		const float c = std::cos(a), s = std::sin(a);
		Mat4 r = identity();
		r.m[0] = c;
		r.m[2] = -s;
		r.m[8] = s;
		r.m[10] = c;
		return r;
	}

	inline Mat4 rotateX(float a)
	{
		const float c = std::cos(a), s = std::sin(a);
		Mat4 r = identity();
		r.m[5] = c;
		r.m[6] = s;
		r.m[9] = -s;
		r.m[10] = c;
		return r;
	}
} // namespace tb
