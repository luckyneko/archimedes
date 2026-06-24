#pragma once

#include "Example.h"
#include "Scene.h"
#include "TbMath.h"

#include <vector>

// The multi-window / threads example: two windows share one compute-deformed mesh (the
// Scene), each rendering it from its own camera. Exercises multi-window + per-window
// render threads, the compute deform + barriers, SSBO vertex pulling, a mixed
// uniform+SSBO+sampler descriptor set, depth, and 4x MSAA.
class RippleMeshExample : public Example
{
public:
	ExampleConfig config() override;
	bool onInit(acm::Device& device, const std::vector<RenderContext*>& views) override;
	void onUpdate(acm::Device& device, float time) override;
	void onRenderView(uint32_t viewIndex, float time) override;
	void onShutdown() override;

private:
	// Per-window render data: its pipeline + descriptor set (this view's camera uniform +
	// the shared scene SSBO + texture) + camera eye.
	struct ViewData
	{
		acm::Pipeline pipeline;
		acm::DescriptorSet descriptor;
		acm::Buffer cameraUniform;
		tb::Vec3 eye;
	};

	std::vector<RenderContext*> m_views; // borrowed (owned by App)
	Scene m_scene;
	acm::DescriptorSetLayout m_layout;
	acm::Buffer m_indexBuffer; // shared scene topology, borrowed for recording
	uint32_t m_indexCount{0};
	std::vector<ViewData> m_viewData;
};
