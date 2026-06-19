#include "InstancedCubesExample.h"

#include "TbGeometry.h"
#include "TbMath.h"
#include "TbUtils.h"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

namespace
{
	// std140 layout matching cube_instanced.vert's Object block: two mat4s + a vec4 = 144B.
	struct ObjectData
	{
		tb::Mat4 mvp;
		tb::Mat4 model;
		float color[4];
	};

	constexpr uint32_t kGrid = 4; // 4x4 cubes
	constexpr float kSpacing = 1.6f;

	// A distinct color per cube index (cycles hues).
	void cubeColor(uint32_t i, float out[4])
	{
		const float h = float(i) * 0.61803f; // golden-ratio hue stepping
		const float t = h - float(int(h));
		// cheap hue ramp
		out[0] = 0.5f + 0.5f * std::cos(6.2831f * (t + 0.0f));
		out[1] = 0.5f + 0.5f * std::cos(6.2831f * (t + 0.33f));
		out[2] = 0.5f + 0.5f * std::cos(6.2831f * (t + 0.66f));
		out[3] = 1.0f;
	}
} // namespace

ExampleConfig InstancedCubesExample::config()
{
	ExampleConfig cfg;
	cfg.windows = {{"Archimedes — Instanced Cubes (dynamic uniforms)", 960, 720, 200, 140}};
	cfg.depth = true;
	cfg.samples = acm::SampleCount::Four;
	return cfg;
}

bool InstancedCubesExample::onInit(acm::Device device, const std::vector<RenderContext*>& views)
{
	m_device = device;
	m_views = views;
	m_cubeCount = kGrid * kGrid;

	// Geometry into device-local vertex/index buffers.
	const std::vector<tb::CubeVertex> verts = tb::cubeVertices();
	const std::vector<uint32_t> indices = tb::cubeIndices();
	const size_t vertexBytes = verts.size() * sizeof(tb::CubeVertex);
	m_vertexBuffer = device.createBuffer(vertexBytes, acm::BufferUsage::Vertex);
	m_indexBuffer = device.createBuffer(indices.size() * sizeof(uint32_t), acm::BufferUsage::Index);
	if (!m_vertexBuffer.valid() || !m_indexBuffer.valid())
		return false;
	m_vertexBuffer.write(verts.data(), vertexBytes);
	m_indexBuffer.write(indices.data(), indices.size() * sizeof(uint32_t));
	m_indexCount = uint32_t(indices.size());

	// One dynamic-uniform slot per cube, each aligned to the device's requirement.
	const size_t align = device.minUniformBufferOffsetAlignment();
	m_stride = ((sizeof(ObjectData) + align - 1) / align) * align;
	m_scratch.resize(m_stride * m_cubeCount);
	m_objectBuffer = device.createBuffer(m_scratch.size(), acm::BufferUsage::Uniform);

	m_layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBufferDynamic, acm::ShaderStage::Vertex},
	});
	if (!m_objectBuffer.valid() || !m_layout.valid())
		return false;
	m_descriptor = device.createDescriptorSet(m_layout);
	if (!m_descriptor.valid())
		return false;
	m_descriptor.setDynamicBuffer(0, m_objectBuffer, sizeof(ObjectData));

	acm::Shader vertex = tb::loadShader(device, "cube_instanced.vert.spv");
	acm::Shader fragment = tb::loadShader(device, "cube_instanced.frag.spv");
	if (!vertex.valid() || !fragment.valid())
		return false;

	acm::PipelineConfig config;
	config.vertex = vertex;
	config.fragment = fragment;
	config.renderPass = m_views[0]->renderPass();
	config.descriptorLayout = m_layout;
	config.depthTest = true;
	config.samples = acm::SampleCount::Four;
	config.cullMode = acm::CullMode::None;
	config.vertexLayout.stride = sizeof(tb::CubeVertex);
	config.vertexLayout.attributes = {
		{0, acm::Format::R32G32B32_Sfloat, offsetof(tb::CubeVertex, pos)},
		{1, acm::Format::R32G32B32_Sfloat, offsetof(tb::CubeVertex, normal)},
	};
	m_pipeline = device.createPipeline(config);
	return m_pipeline.valid();
}

void InstancedCubesExample::onUpdate(acm::Device device, float time)
{
	// The per-cube buffer is rewritten every frame and read by the in-flight draws; idle
	// the device first so we don't clobber a frame still being read. (A simple-correct
	// baseline; a per-frame ring would remove the stall — see WORK.md.)
	device.waitIdle();

	const acm::Extent2D extent = m_views[0]->extent();
	const float aspect = float(extent.width) / float(extent.height > 0 ? extent.height : 1);
	const tb::Mat4 proj = tb::perspective(0.7f, aspect, 0.1f, 50.0f);
	const tb::Mat4 view = tb::lookAt({0.0f, 5.0f, 8.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});

	const float half = float(kGrid - 1) * 0.5f;
	for (uint32_t i = 0; i < m_cubeCount; ++i)
	{
		const uint32_t gx = i % kGrid;
		const uint32_t gz = i / kGrid;
		const tb::Vec3 pos{(float(gx) - half) * kSpacing, 0.0f, (float(gz) - half) * kSpacing};
		const float rate = 0.5f + 0.08f * float(i);
		const tb::Mat4 model = tb::translate(pos) * tb::rotateY(time * rate) * tb::rotateX(time * rate * 0.6f);

		ObjectData obj;
		obj.mvp = proj * view * model;
		obj.model = model;
		cubeColor(i, obj.color);
		std::memcpy(m_scratch.data() + i * m_stride, &obj, sizeof(obj));
	}
	m_objectBuffer.write(m_scratch.data(), m_scratch.size());
}

void InstancedCubesExample::onRenderView(uint32_t viewIndex, float)
{
	m_views[viewIndex]->renderer().render([&](acm::CommandBuffer cmd, uint32_t)
										  {
											  cmd.bindPipeline(m_pipeline);
											  cmd.bindVertexBuffer(m_vertexBuffer);
											  cmd.bindIndexBuffer(m_indexBuffer);
											  for (uint32_t i = 0; i < m_cubeCount; ++i)
											  {
												  cmd.bindDescriptorSet(m_pipeline, m_descriptor, uint32_t(i * m_stride));
												  cmd.drawIndexed(m_indexCount);
											  } });
}

void InstancedCubesExample::onShutdown()
{
	m_pipeline.reset();
	m_descriptor.reset();
	m_layout.reset();
	m_objectBuffer.reset();
	m_indexBuffer.reset();
	m_vertexBuffer.reset();
	m_device.reset();
}
