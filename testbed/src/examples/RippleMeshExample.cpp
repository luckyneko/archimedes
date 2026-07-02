/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "RippleMeshExample.h"

#include "TbUtils.h"

#include <array>

namespace
{
	// What mesh.vert's uniform block expects: MVP for positions, model for normals (the
	// mesh is world-space, so model is identity — it just passes normals through).
	struct CameraUniform
	{
		tb::Mat4 mvp;
		tb::Mat4 model;
	};

	// Two viewpoints of the same mesh.
	const std::array<tb::Vec3, 2> kEyes = {
		tb::Vec3{2.4f, 2.0f, 2.4f}, // high oblique
		tb::Vec3{0.0f, 0.7f, 3.4f}, // low, head-on
	};
} // namespace

ExampleConfig RippleMeshExample::config()
{
	ExampleConfig cfg;
	cfg.windows = {
		{"Archimedes — Ripple (Perspective)", 640, 480, 120, 200},
		{"Archimedes — Ripple (Side)", 640, 480, 800, 200},
	};
	cfg.depth = true;
	cfg.samples = acm::SampleCount::Four;
	return cfg;
}

bool RippleMeshExample::onInit(acm::Device& device, const std::vector<RenderContext*>& views)
{
	m_views = views;

	acm::Shader vertex = tb::loadShader(device, "mesh.vert.spv");
	acm::Shader fragment = tb::loadShader(device, "mesh.frag.spv");
	acm::Shader compute = tb::loadShader(device, "mesh.comp.spv");
	if (!vertex.valid() || !fragment.valid() || !compute.valid())
		return false;

	if (!m_scene.init(device, compute))
		return false;
	m_indexBuffer = m_scene.indexBuffer();
	m_indexCount = m_scene.indexCount();

	// One set per window: this view's camera uniform (binding 0, vertex) + the shared
	// mesh SSBO (binding 1, vertex) + the shared texture (binding 2, fragment).
	m_layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Vertex},
		{1, acm::DescriptorType::StorageBuffer, acm::ShaderStage::Vertex},
		{2, acm::DescriptorType::CombinedImageSampler, acm::ShaderStage::Fragment},
	});
	if (!m_layout.valid())
		return false;

	m_viewData.resize(views.size());
	for (size_t i = 0; i < views.size(); ++i)
	{
		ViewData& vd = m_viewData[i];
		vd.eye = kEyes[i < kEyes.size() ? i : kEyes.size() - 1];

		vd.cameraUniform = device.createBuffer(sizeof(CameraUniform), acm::BufferUsage::Uniform);
		vd.descriptor = device.createDescriptorSet(m_layout);
		if (!vd.cameraUniform.valid() || !vd.descriptor.valid())
			return false;
		vd.descriptor.setBuffer(0, vd.cameraUniform);
		vd.descriptor.setBuffer(1, m_scene.vertexBuffer());
		vd.descriptor.setTexture(2, m_scene.texture(), m_scene.sampler());

		acm::PipelineConfig config;
		config.vertex = vertex;
		config.fragment = fragment;
		config.target = m_views[i]->renderTarget();
		config.descriptorLayout = m_layout;
		config.depthTest = true;
		config.cullMode = acm::CullMode::None; // the rippling grid is viewed from both sides
		// Empty vertexLayout: geometry comes from the SSBO via gl_VertexIndex.
		vd.pipeline = device.createPipeline(config);
		if (!vd.pipeline.valid())
			return false;
	}
	return true;
}

void RippleMeshExample::onUpdate(acm::Device&, float time)
{
	m_scene.update(time); // GPU compute deform (barrier-fenced against the prior frame)
}

void RippleMeshExample::onRenderView(uint32_t viewIndex, float)
{
	ViewData& vd = m_viewData[viewIndex];

	const acm::Extent2D extent = m_views[viewIndex]->extent();
	const float aspect = float(extent.width) / float(extent.height > 0 ? extent.height : 1);
	const tb::Mat4 view = tb::lookAt(vd.eye, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
	const tb::Mat4 proj = tb::perspective(0.9f, aspect, 0.1f, 20.0f);
	CameraUniform cam;
	cam.mvp = proj * view;
	cam.model = tb::identity();
	vd.cameraUniform.write(&cam, sizeof(cam));

	m_views[viewIndex]->renderer().render([&](acm::CommandBuffer& cmd, uint32_t)
										  {
											  cmd.bindPipeline(vd.pipeline);
											  cmd.bindDescriptorSet(vd.pipeline, vd.descriptor);
											  cmd.bindIndexBuffer(m_indexBuffer);
											  cmd.drawIndexed(m_indexCount); });
}

void RippleMeshExample::onShutdown()
{
	m_viewData.clear(); // pipelines / descriptors / camera uniforms
	m_indexBuffer.reset();
	m_layout.reset();
	m_scene.shutdown();
}
