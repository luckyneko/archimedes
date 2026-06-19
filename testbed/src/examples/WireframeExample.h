#pragma once

#include "Example.h"
#include <vector>

// Polygon mode + wide lines: a spinning cube drawn as wireframe (PolygonMode::Line,
// lineWidth > 1). Both are feature-gated — the pipeline falls back to fill / width 1 and
// warns when fillModeNonSolid / wideLines are unavailable, exercising the
// degrade-with-a-warning path. Single window, depth + 4x MSAA (so the lines resolve
// cleanly).
class WireframeExample : public Example
{
public:
	ExampleConfig config() override;
	bool onInit(acm::Device device, const std::vector<RenderContext*>& views) override;
	void onUpdate(acm::Device device, float time) override;
	void onRenderView(uint32_t viewIndex, float time) override;
	void onShutdown() override;

private:
	acm::Device m_device;
	std::vector<RenderContext*> m_views;

	acm::Buffer m_vertexBuffer;
	acm::Buffer m_indexBuffer;
	uint32_t m_indexCount{0};
	acm::Buffer m_uniform; // { mvp, model, color }
	acm::DescriptorSetLayout m_layout;
	acm::DescriptorSet m_descriptor;
	acm::Pipeline m_pipeline;
};
