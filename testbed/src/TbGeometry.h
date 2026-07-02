/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <cstdint>
#include <vector>

// Shared testbed geometry. A unit cube (position + normal) used by several examples.
namespace tb
{
	struct CubeVertex
	{
		float pos[3];
		float normal[3];
	};

	// 24 vertices: 4 per face, each with the face's outward normal.
	inline std::vector<CubeVertex> cubeVertices()
	{
		return {
			{{-0.5f, -0.5f, 0.5f}, {0, 0, 1}},
			{{0.5f, -0.5f, 0.5f}, {0, 0, 1}},
			{{0.5f, 0.5f, 0.5f}, {0, 0, 1}},
			{{-0.5f, 0.5f, 0.5f}, {0, 0, 1}},
			{{0.5f, -0.5f, -0.5f}, {0, 0, -1}},
			{{-0.5f, -0.5f, -0.5f}, {0, 0, -1}},
			{{-0.5f, 0.5f, -0.5f}, {0, 0, -1}},
			{{0.5f, 0.5f, -0.5f}, {0, 0, -1}},
			{{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}},
			{{-0.5f, -0.5f, 0.5f}, {-1, 0, 0}},
			{{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}},
			{{-0.5f, 0.5f, -0.5f}, {-1, 0, 0}},
			{{0.5f, -0.5f, 0.5f}, {1, 0, 0}},
			{{0.5f, -0.5f, -0.5f}, {1, 0, 0}},
			{{0.5f, 0.5f, -0.5f}, {1, 0, 0}},
			{{0.5f, 0.5f, 0.5f}, {1, 0, 0}},
			{{-0.5f, 0.5f, 0.5f}, {0, 1, 0}},
			{{0.5f, 0.5f, 0.5f}, {0, 1, 0}},
			{{0.5f, 0.5f, -0.5f}, {0, 1, 0}},
			{{-0.5f, 0.5f, -0.5f}, {0, 1, 0}},
			{{-0.5f, -0.5f, -0.5f}, {0, -1, 0}},
			{{0.5f, -0.5f, -0.5f}, {0, -1, 0}},
			{{0.5f, -0.5f, 0.5f}, {0, -1, 0}},
			{{-0.5f, -0.5f, 0.5f}, {0, -1, 0}},
		};
	}

	// 36 indices: two triangles per face.
	inline std::vector<uint32_t> cubeIndices()
	{
		std::vector<uint32_t> indices;
		indices.reserve(36);
		for (uint32_t face = 0; face < 6; ++face)
		{
			const uint32_t b = face * 4;
			indices.insert(indices.end(), {b + 0, b + 1, b + 2, b + 0, b + 2, b + 3});
		}
		return indices;
	}
} // namespace tb
