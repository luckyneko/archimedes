#pragma once

#include "Example.h"

#include <vector>

// Render-to-texture. A spinning cube is rendered into an offscreen RenderTarget
// (RenderTargetFinish::Sampled) during the renderer's pre-pass, then that texture is
// sampled fullscreen in the main pass — two render passes in one command buffer.
// Exercises the offscreen RenderTarget path (its own render pass + depth) and sampling an
// RTT result. Single window. The shared offscreen texture is rewritten + read each frame,
// so onUpdate idles the device first (simple-correct; a ring would remove the stall).
class MirrorExample : public Example
{
public:
	ExampleConfig config() override;
	bool onInit(acm::Device& device, const std::vector<RenderContext*>& views) override;
	void onUpdate(acm::Device& device, float time) override;
	void onRenderView(uint32_t viewIndex, float time) override;
	void onShutdown() override;

private:
	static constexpr uint32_t kRttSize = 512;

	std::vector<RenderContext*> m_views;

	// Offscreen pass: a cube into a sampled texture.
	acm::Texture m_offscreenColor;
	acm::RenderTarget m_offscreenTarget;
	acm::Buffer m_cubeVertices;
	acm::Buffer m_cubeIndices;
	uint32_t m_cubeIndexCount{0};
	acm::Buffer m_cubeUniform; // { mvp, model, color }
	acm::DescriptorSetLayout m_cubeLayout;
	acm::DescriptorSet m_cubeDescriptor;
	acm::Pipeline m_cubePipeline;

	// Main pass: sample the offscreen texture fullscreen.
	acm::Sampler m_sampler;
	acm::DescriptorSetLayout m_quadLayout;
	acm::DescriptorSet m_quadDescriptor;
	acm::Pipeline m_quadPipeline;
};
