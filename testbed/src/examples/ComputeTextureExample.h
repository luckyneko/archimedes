#pragma once

#include "Example.h"
#include <array>
#include <vector>

// Storage image + compute-in-the-render-frame. Each frame a compute shader writes an
// animated plasma into a storage image during the renderer's pre-pass (no separate
// submit), then a fullscreen draw samples it in the same command buffer. Exercises
// DescriptorType::StorageImage, setStorageImage, transitionImage, and
// Renderer::render(prePass, record) — the first live-driver use of the compute-in-frame
// hook. A per-frame-in-flight ring of the time uniform keeps it correct without a
// device wait-idle; the shared storage image is synced across frames by the layout
// transitions (which are submission-order barriers). Single window.
class ComputeTextureExample : public Example
{
public:
	ExampleConfig config() override;
	bool onInit(acm::Device device, const std::vector<RenderContext*>& views) override;
	void onUpdate(acm::Device device, float time) override;
	void onRenderView(uint32_t viewIndex, float time) override;
	void onShutdown() override;

private:
	static constexpr uint32_t kFrames = acm::Renderer::MaxFramesInFlight;
	static constexpr uint32_t kImageSize = 512; // multiple of the 8x8 workgroup

	acm::Device m_device;
	std::vector<RenderContext*> m_views;

	acm::Texture m_image; // storage image the compute writes + the draw samples
	acm::Sampler m_sampler;

	acm::ComputePipeline m_compute;
	acm::DescriptorSetLayout m_computeLayout;
	std::array<acm::DescriptorSet, kFrames> m_computeSets; // one per in-flight slot
	std::array<acm::Buffer, kFrames> m_timeUniforms;	   // per-slot, ringed

	acm::Pipeline m_graphics;
	acm::DescriptorSetLayout m_graphicsLayout;
	acm::DescriptorSet m_graphicsSet;

	float m_time{0.0f};
};
