#include "MirrorExample.h"

#include "TbGeometry.h"
#include "TbMath.h"
#include "TbUtils.h"
#include <cstring>
#include <vector>

namespace
{
	// Matches cube_instanced.vert's Object block (used here with a plain, non-dynamic
	// uniform — the shader doesn't care). std140: two mat4s + a vec4 = 144 bytes.
	struct CubeUniform
	{
		tb::Mat4 mvp;
		tb::Mat4 model;
		float color[4];
	};
} // namespace

ExampleConfig MirrorExample::config()
{
	ExampleConfig cfg;
	cfg.windows = {{"Archimedes — Mirror (render-to-texture)", 768, 768, 240, 120}};
	cfg.depth = false; // the main pass just samples a fullscreen quad
	cfg.samples = acm::SampleCount::One;
	return cfg;
}

bool MirrorExample::onInit(acm::Device device, const std::vector<RenderContext*>& views)
{
	m_device = device;
	m_views = views;

	// Offscreen color texture + its render target (owns a render pass; depth for the cube).
	m_offscreenColor = device.createTexture(acm::Format::B8G8R8A8_Unorm, acm::Extent2D{kRttSize, kRttSize});
	m_offscreenTarget = device.createRenderTarget(m_offscreenColor, acm::RenderTargetFinish::Sampled, /*depth*/ true);
	if (!m_offscreenColor.valid() || !m_offscreenTarget.valid())
		return false;

	// Cube geometry + a single transform/color uniform.
	const std::vector<tb::CubeVertex> verts = tb::cubeVertices();
	const std::vector<uint32_t> indices = tb::cubeIndices();
	const size_t vertexBytes = verts.size() * sizeof(tb::CubeVertex);
	m_cubeVertices = device.createBuffer(vertexBytes, acm::BufferUsage::Vertex);
	m_cubeIndices = device.createBuffer(indices.size() * sizeof(uint32_t), acm::BufferUsage::Index);
	m_cubeUniform = device.createBuffer(sizeof(CubeUniform), acm::BufferUsage::Uniform);
	if (!m_cubeVertices.valid() || !m_cubeIndices.valid() || !m_cubeUniform.valid())
		return false;
	m_cubeVertices.write(verts.data(), vertexBytes);
	m_cubeIndices.write(indices.data(), indices.size() * sizeof(uint32_t));
	m_cubeIndexCount = uint32_t(indices.size());

	m_cubeLayout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Vertex},
	});
	if (!m_cubeLayout.valid())
		return false;
	m_cubeDescriptor = device.createDescriptorSet(m_cubeLayout);
	if (!m_cubeDescriptor.valid())
		return false;
	m_cubeDescriptor.setBuffer(0, m_cubeUniform);

	acm::PipelineConfig cubeCfg;
	cubeCfg.vertex = tb::loadShader(device, "cube_instanced.vert.spv");
	cubeCfg.fragment = tb::loadShader(device, "cube_instanced.frag.spv");
	cubeCfg.renderPass = m_offscreenTarget.vkRenderPass();
	cubeCfg.descriptorLayout = m_cubeLayout;
	cubeCfg.depthTest = true;
	cubeCfg.cullMode = acm::CullMode::None;
	cubeCfg.vertexLayout.stride = sizeof(tb::CubeVertex);
	cubeCfg.vertexLayout.attributes = {
		{0, acm::Format::R32G32B32_Sfloat, offsetof(tb::CubeVertex, pos)},
		{1, acm::Format::R32G32B32_Sfloat, offsetof(tb::CubeVertex, normal)},
	};
	m_cubePipeline = device.createPipeline(cubeCfg);
	if (!m_cubePipeline.valid())
		return false;

	// Main pass: a fullscreen quad sampling the offscreen texture.
	m_sampler = device.createSampler();
	m_quadLayout = device.createDescriptorSetLayout(1); // one fragment sampler at binding 0
	if (!m_sampler.valid() || !m_quadLayout.valid())
		return false;
	m_quadDescriptor = device.createDescriptorSet(m_quadLayout);
	if (!m_quadDescriptor.valid())
		return false;
	m_quadDescriptor.setTexture(0, m_offscreenColor, m_sampler);

	acm::PipelineConfig quadCfg;
	quadCfg.vertex = tb::loadShader(device, "fullscreen.vert.spv");
	quadCfg.fragment = tb::loadShader(device, "sample.frag.spv");
	quadCfg.renderPass = m_views[0]->renderPass();
	quadCfg.descriptorLayout = m_quadLayout;
	m_quadPipeline = device.createPipeline(quadCfg);
	return m_quadPipeline.valid();
}

void MirrorExample::onUpdate(acm::Device device, float time)
{
	// The single offscreen texture is written + sampled each frame; idle first so this
	// frame's render doesn't clobber the previous frame's still-in-flight sample.
	device.waitIdle();

	CubeUniform cam;
	const tb::Mat4 model = tb::rotateY(time * 0.8f) * tb::rotateX(time * 0.5f);
	const tb::Mat4 view = tb::lookAt({0.0f, 1.2f, 2.6f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
	const tb::Mat4 proj = tb::perspective(0.9f, 1.0f /* square RTT */, 0.1f, 10.0f);
	cam.mvp = proj * view * model;
	cam.model = model;
	cam.color[0] = 0.95f;
	cam.color[1] = 0.6f;
	cam.color[2] = 0.25f;
	cam.color[3] = 1.0f;
	m_cubeUniform.write(&cam, sizeof(cam));
}

void MirrorExample::onRenderView(uint32_t viewIndex, float)
{
	m_views[viewIndex]->renderer().render(
		[&](acm::CommandBuffer cmd, uint32_t) // pre-pass: render the cube into the offscreen texture
		{
			cmd.beginRenderPass(m_offscreenTarget, 0.06f, 0.10f, 0.12f, 1.0f);
			cmd.setViewportAndScissor(acm::Extent2D{kRttSize, kRttSize});
			cmd.bindPipeline(m_cubePipeline);
			cmd.bindDescriptorSet(m_cubePipeline, m_cubeDescriptor);
			cmd.bindVertexBuffer(m_cubeVertices);
			cmd.bindIndexBuffer(m_cubeIndices);
			cmd.drawIndexed(m_cubeIndexCount);
			cmd.endRenderPass(); // RenderTargetFinish::Sampled leaves it SHADER_READ_ONLY
		},
		[&](acm::CommandBuffer cmd, uint32_t) // main pass: sample it fullscreen
		{
			cmd.bindPipeline(m_quadPipeline);
			cmd.bindDescriptorSet(m_quadPipeline, m_quadDescriptor);
			cmd.draw(3);
		});
}

void MirrorExample::onShutdown()
{
	m_quadPipeline.reset();
	m_quadDescriptor.reset();
	m_quadLayout.reset();
	m_sampler.reset();
	m_cubePipeline.reset();
	m_cubeDescriptor.reset();
	m_cubeLayout.reset();
	m_cubeUniform.reset();
	m_cubeIndices.reset();
	m_cubeVertices.reset();
	m_offscreenTarget.reset();
	m_offscreenColor.reset();
	m_device.reset();
}
