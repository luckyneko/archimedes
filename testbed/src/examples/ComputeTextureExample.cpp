/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "ComputeTextureExample.h"

#include "TbUtils.h"

ExampleConfig ComputeTextureExample::config()
{
	ExampleConfig cfg;
	cfg.windows = {{"Archimedes — Compute Texture (storage image + pre-pass)", 768, 768, 240, 120}};
	cfg.depth = false; // a fullscreen blit needs no depth
	cfg.samples = acm::SampleCount::One;
	return cfg;
}

bool ComputeTextureExample::onInit(acm::Device& device, const std::vector<RenderContext*>& views)
{
	m_views = views;

	// The storage image the compute writes and the draw samples.
	m_image = device.createTexture(acm::Format::R8G8B8A8_Unorm, acm::Extent2D{kImageSize, kImageSize}, acm::TextureConfig{false, true});
	m_sampler = device.createSampler();
	if (!m_image.valid() || !m_sampler.valid())
		return false;

	// Leave it in SHADER_READ_ONLY so every frame's pre-pass can uniformly do
	// ShaderReadOnly -> General (write) -> ShaderReadOnly (sample).
	device.submitSync([&](acm::CommandBuffer& cmd)
					  { cmd.transitionImage(m_image, acm::ImageLayout::Undefined, acm::ImageLayout::ShaderReadOnly); });

	// Compute side: storage image (0) + a ringed time uniform (1).
	m_computeLayout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::StorageImage, acm::ShaderStage::Compute},
		{1, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Compute},
	});
	if (!m_computeLayout.valid())
		return false;
	acm::Shader computeShader = tb::loadShader(device, "plasma.comp.spv");
	m_compute = device.createComputePipeline(computeShader, m_computeLayout);
	if (!m_compute.valid())
		return false;
	for (uint32_t f = 0; f < kFrames; ++f)
	{
		m_timeUniforms[f] = device.createBuffer(sizeof(float) * 4, acm::BufferUsage::Uniform);
		m_computeSets[f] = device.createDescriptorSet(m_computeLayout);
		if (!m_timeUniforms[f].valid() || !m_computeSets[f].valid())
			return false;
		m_computeSets[f].setStorageImage(0, m_image);
		m_computeSets[f].setBuffer(1, m_timeUniforms[f]);
	}

	// Graphics side: a fullscreen draw sampling the image.
	m_graphicsLayout = device.createDescriptorSetLayout(1); // one fragment sampler at binding 0
	if (!m_graphicsLayout.valid())
		return false;
	m_graphicsSet = device.createDescriptorSet(m_graphicsLayout);
	if (!m_graphicsSet.valid())
		return false;
	m_graphicsSet.setTexture(0, m_image, m_sampler);

	acm::PipelineShaders shaders;
	shaders.vertex = tb::loadShader(device, "fullscreen.vert.spv");
	shaders.fragment = tb::loadShader(device, "sample.frag.spv");
	acm::PipelineConfig config;
	config.descriptorLayout = m_graphicsLayout;
	m_graphics = device.createPipeline(shaders, m_views[0]->renderTarget(), config);
	return m_graphics.valid();
}

void ComputeTextureExample::onUpdate(acm::Device&, float time)
{
	m_time = time; // consumed in the pre-pass (which knows the in-flight slot)
}

void ComputeTextureExample::onRenderView(uint32_t viewIndex, float)
{
	m_views[viewIndex]->renderer().render(
		[&](acm::CommandBuffer& cmd, uint32_t frame) // pre-pass: compute the image
		{
			// Write this slot's time uniform (safe: the renderer waited this slot's fence).
			const float params[4] = {m_time, float(kImageSize), float(kImageSize), 0.0f};
			m_timeUniforms[frame].write(params, sizeof(params));

			cmd.transitionImage(m_image, acm::ImageLayout::ShaderReadOnly, acm::ImageLayout::General);
			cmd.bindComputePipeline(m_compute);
			cmd.bindComputeDescriptorSet(m_compute, m_computeSets[frame]);
			cmd.dispatch(kImageSize / 8, kImageSize / 8, 1);
			cmd.transitionImage(m_image, acm::ImageLayout::General, acm::ImageLayout::ShaderReadOnly);
		},
		[&](acm::CommandBuffer& cmd, uint32_t) // draws: sample it fullscreen
		{
			cmd.bindPipeline(m_graphics);
			cmd.bindDescriptorSet(m_graphics, m_graphicsSet);
			cmd.draw(3);
		});
}

void ComputeTextureExample::onShutdown()
{
	m_graphics.reset();
	m_graphicsSet.reset();
	m_graphicsLayout.reset();
	for (auto& s : m_computeSets)
		s.reset();
	for (auto& b : m_timeUniforms)
		b.reset();
	m_compute.reset();
	m_computeLayout.reset();
	m_sampler.reset();
	m_image.reset();
}
