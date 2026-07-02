/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "ExampleRegistry.h"

#include "Example.h"
#include "examples/ComputeTextureExample.h"
#include "examples/InstancedCubesExample.h"
#include "examples/MirrorExample.h"
#include "examples/RippleMeshExample.h"
#include "examples/SpriteAtlasExample.h"
#include "examples/WireframeExample.h"

#include <cstdio>
#include <vector>

namespace
{
	template <typename T>
	std::unique_ptr<Example> make()
	{
		return std::make_unique<T>();
	}

	struct Entry
	{
		std::string name;
		std::unique_ptr<Example> (*factory)();
		std::string description;
	};

	const std::vector<Entry>& registry()
	{
		static const std::vector<Entry> entries = {
			{"ripple-mesh", &make<RippleMeshExample>, "two windows, one shared compute-deformed mesh (multi-window + render threads)"},
			{"instanced-cubes", &make<InstancedCubesExample>, "a grid of spinning cubes from one dynamic uniform buffer (per-draw offset)"},
			{"compute-texture", &make<ComputeTextureExample>, "a compute shader writes an animated storage image each frame (render pre-pass), sampled fullscreen"},
			{"mirror", &make<MirrorExample>, "render a spinning cube to an offscreen texture (pre-pass), then sample it fullscreen (render-to-texture)"},
			{"sprite-atlas", &make<SpriteAtlasExample>, "overlapping translucent 2D sprites from a 3-texture descriptor array (alpha blend)"},
			{"wireframe", &make<WireframeExample>, "a spinning wireframe cube (polygon mode Line + wide lines, feature-gated)"},
		};
		return entries;
	}
} // namespace

std::unique_ptr<Example> makeExample(const std::string& name)
{
	for (const Entry& e : registry())
		if (e.name == name)
			return e.factory();
	return nullptr;
}

const char* defaultExampleName()
{
	return "ripple-mesh";
}

void listExamples()
{
	printf("Available examples (pass one as the first argument):\n");
	for (const Entry& e : registry())
		printf("  %-16s %s\n", e.name.c_str(), e.description.c_str());
}
