/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "WireframeExample.h"

#include "TbGeometry.h"
#include "TbMath.h"
#include "TbUtils.h"

#include <vector>

namespace
{
	// Matches cube_instanced.vert's Object block (plain uniform here). 144 bytes.
	struct CubeUniform
	{
		tb::Mat4 mvp;
		tb::Mat4 model;
		float color[4];
	};
} // namespace

ExampleConfig WireframeExample::config()
{
	ExampleConfig cfg;
	cfg.windows = {{"Archimedes — Wireframe (polygon mode + wide lines)", 800, 800, 240, 120}};
	cfg.depth = true;
	cfg.samples = acm::SampleCount::Four;
	return cfg;
}

bool WireframeExample::onInit(acm::Device& device, const std::vector<RenderContext*>& views)
{
	m_views = views;

	const std::vector<tb::CubeVertex> verts = tb::cubeVertices();
	const std::vector<uint32_t> indices = tb::cubeIndices();
	const size_t vertexBytes = verts.size() * sizeof(tb::CubeVertex);
	m_vertexBuffer = device.createBuffer(vertexBytes, acm::BufferUsage::Vertex);
	m_indexBuffer = device.createBuffer(indices.size() * sizeof(uint32_t), acm::BufferUsage::Index);
	m_uniform = device.createBuffer(sizeof(CubeUniform), acm::BufferUsage::Uniform);
	if (!m_vertexBuffer.valid() || !m_indexBuffer.valid() || !m_uniform.valid())
		return false;
	m_vertexBuffer.write(verts.data(), vertexBytes);
	m_indexBuffer.write(indices.data(), indices.size() * sizeof(uint32_t));
	m_indexCount = uint32_t(indices.size());

	m_layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Vertex},
	});
	if (!m_layout.valid())
		return false;
	m_descriptor = device.createDescriptorSet(m_layout);
	if (!m_descriptor.valid())
		return false;
	m_descriptor.setBuffer(0, m_uniform);

	acm::PipelineConfig config;
	config.vertex = tb::loadShader(device, "cube_instanced.vert.spv");
	config.fragment = tb::loadShader(device, "cube_instanced.frag.spv");
	config.target = m_views[0]->renderTarget();
	config.descriptorLayout = m_layout;
	config.depthTest = true;
	config.cullMode = acm::CullMode::None;		 // see all edges
	config.polygonMode = acm::PolygonMode::Line; // wireframe (feature-gated)
	config.lineWidth = 2.0f;					 // wide lines (feature-gated)
	config.vertexLayout.stride = sizeof(tb::CubeVertex);
	config.vertexLayout.attributes = {
		{0, acm::Format::R32G32B32_Sfloat, offsetof(tb::CubeVertex, pos)},
		{1, acm::Format::R32G32B32_Sfloat, offsetof(tb::CubeVertex, normal)},
	};
	m_pipeline = device.createPipeline(config);
	return m_pipeline.valid();
}

void WireframeExample::onUpdate(acm::Device& device, float time)
{
	device.waitIdle(); // single uniform rewritten each frame — idle before clobbering it

	const acm::Extent2D extent = m_views[0]->extent();
	const float aspect = float(extent.width) / float(extent.height > 0 ? extent.height : 1);
	const tb::Mat4 model = tb::rotateY(time * 0.7f) * tb::rotateX(time * 0.45f);
	const tb::Mat4 view = tb::lookAt({0.0f, 1.0f, 2.6f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
	const tb::Mat4 proj = tb::perspective(0.9f, aspect, 0.1f, 10.0f);
	CubeUniform u;
	u.mvp = proj * view * model;
	u.model = model;
	u.color[0] = 0.2f;
	u.color[1] = 1.0f;
	u.color[2] = 0.9f;
	u.color[3] = 1.0f;
	m_uniform.write(&u, sizeof(u));
}

void WireframeExample::onRenderView(uint32_t viewIndex, float)
{
	m_views[viewIndex]->renderer().render([&](acm::CommandBuffer& cmd, uint32_t)
										  {
											  cmd.bindPipeline(m_pipeline);
											  cmd.bindDescriptorSet(m_pipeline, m_descriptor);
											  cmd.bindVertexBuffer(m_vertexBuffer);
											  cmd.bindIndexBuffer(m_indexBuffer);
											  cmd.drawIndexed(m_indexCount); });
}

void WireframeExample::onShutdown()
{
	m_pipeline.reset();
	m_descriptor.reset();
	m_layout.reset();
	m_uniform.reset();
	m_indexBuffer.reset();
	m_vertexBuffer.reset();
}
