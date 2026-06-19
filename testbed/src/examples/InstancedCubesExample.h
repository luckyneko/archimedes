#pragma once

#include "Example.h"
#include <cstdint>
#include <vector>

// Dynamic uniform buffers: a grid of spinning cubes, each pulling its transform + color
// from one shared uniform buffer via a per-draw byte offset. Exercises
// DescriptorType::UniformBufferDynamic, setDynamicBuffer, bindDescriptorSet(.., offset),
// minUniformBufferOffsetAlignment, plus real vertex/index buffers with a pos+normal
// vertex layout, depth, and 4x MSAA. Single window.
class InstancedCubesExample : public Example
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

	acm::DescriptorSetLayout m_layout;
	acm::DescriptorSet m_descriptor;
	acm::Buffer m_objectBuffer; // one dynamic-uniform slot per cube
	acm::Pipeline m_pipeline;

	uint32_t m_cubeCount{0};
	size_t m_stride{0};				// aligned bytes per cube slot
	std::vector<uint8_t> m_scratch; // CPU staging for the per-cube constants
};
