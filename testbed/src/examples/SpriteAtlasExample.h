/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "Example.h"

#include <array>
#include <cstdint>
#include <vector>

// Descriptor arrays + alpha blending in a 2D ortho scene. A handful of overlapping,
// translucent sprites are drawn from one dynamic uniform buffer (per-sprite rect / tint /
// texture index), each sampling one element of a 3-texture descriptor array. Exercises
// DescriptorType::CombinedImageSampler with count > 1 (array), BlendMode::AlphaBlend,
// triangle-strip topology, and dynamic uniforms together. Single window, no depth.
class SpriteAtlasExample : public Example
{
public:
	ExampleConfig config() override;
	bool onInit(acm::Device& device, const std::vector<RenderContext*>& views) override;
	void onUpdate(acm::Device& device, float time) override;
	void onRenderView(uint32_t viewIndex, float time) override;
	void onShutdown() override;

private:
	static constexpr uint32_t kTextureCount = 3;

	std::vector<RenderContext*> m_views;

	std::array<acm::Texture, kTextureCount> m_textures;
	acm::Sampler m_sampler;

	acm::DescriptorSetLayout m_layout;
	acm::DescriptorSet m_descriptor;
	acm::Buffer m_spriteBuffer; // one dynamic-uniform slot per sprite
	acm::Pipeline m_pipeline;

	uint32_t m_spriteCount{0};
	size_t m_stride{0};
};
