#include "SpriteAtlasExample.h"

#include "TbUtils.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace
{
	// std140 per-sprite block matching sprite.vert: vec4 + vec4 + ivec4 = 48 bytes.
	struct SpriteData
	{
		float rect[4];	 // centerX, centerY, halfW, halfH (NDC)
		float tint[4];	 // rgba
		int32_t misc[4]; // x = texture index
	};

	constexpr uint32_t kSize = 64; // procedural texture size

	float smoothstepf(float a, float b, float x)
	{
		const float t = (x - a) / (b - a);
		const float c = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
		return c * c * (3.0f - 2.0f * c);
	}

	// Three procedural RGBA textures with alpha, so the blending is obvious. [R,G,B,A].
	std::vector<uint8_t> makeTexture(uint32_t which)
	{
		std::vector<uint8_t> px(size_t(kSize) * kSize * 4);
		for (uint32_t y = 0; y < kSize; ++y)
			for (uint32_t x = 0; x < kSize; ++x)
			{
				const float u = (float(x) + 0.5f) / kSize;
				const float v = (float(y) + 0.5f) / kSize;
				float r = 1, g = 1, b = 1, a = 1;
				if (which == 0) // soft white disc (alpha falloff)
				{
					const float d = std::sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
					a = 1.0f - smoothstepf(0.35f, 0.5f, d);
				}
				else if (which == 1) // checker, fully opaque cells / transparent gaps
				{
					const bool cell = ((int(u * 4) + int(v * 4)) & 1) != 0;
					a = cell ? 1.0f : 0.15f;
				}
				else // diagonal gradient, semi-transparent
				{
					r = u;
					g = v;
					b = 1.0f - u;
					a = 0.7f;
				}
				uint8_t* p = &px[(size_t(y) * kSize + x) * 4];
				p[0] = uint8_t(r * 255.0f);
				p[1] = uint8_t(g * 255.0f);
				p[2] = uint8_t(b * 255.0f);
				p[3] = uint8_t(a * 255.0f);
			}
		return px;
	}

	// A fixed layout of overlapping sprites: {center, half-size, tint rgba, texIndex}.
	const std::vector<SpriteData> kSprites = {
		{{-0.30f, -0.20f, 0.45f, 0.45f}, {1.0f, 0.4f, 0.3f, 0.85f}, {0, 0, 0, 0}},
		{{0.25f, 0.10f, 0.40f, 0.40f}, {0.3f, 0.9f, 0.5f, 0.80f}, {2, 0, 0, 0}},
		{{0.05f, -0.05f, 0.50f, 0.30f}, {0.4f, 0.6f, 1.0f, 0.75f}, {1, 0, 0, 0}},
		{{-0.45f, 0.35f, 0.30f, 0.30f}, {1.0f, 0.9f, 0.3f, 0.90f}, {0, 0, 0, 0}},
		{{0.45f, -0.40f, 0.35f, 0.35f}, {0.9f, 0.5f, 1.0f, 0.70f}, {2, 0, 0, 0}},
	};
} // namespace

ExampleConfig SpriteAtlasExample::config()
{
	ExampleConfig cfg;
	cfg.windows = {{"Archimedes — Sprite Atlas (descriptor array + alpha blend)", 800, 800, 240, 120}};
	cfg.depth = false;
	cfg.samples = acm::SampleCount::One;
	return cfg;
}

bool SpriteAtlasExample::onInit(acm::Device& device, const std::vector<RenderContext*>& views)
{
	m_views = views;
	m_spriteCount = uint32_t(kSprites.size());

	for (uint32_t i = 0; i < kTextureCount; ++i)
	{
		m_textures[i] = device.createTexture(acm::Format::R8G8B8A8_Unorm, acm::Extent2D{kSize, kSize});
		if (!m_textures[i].valid())
			return false;
		const std::vector<uint8_t> px = makeTexture(i);
		m_textures[i].upload(px.data(), px.size());
	}
	m_sampler = device.createSampler();
	if (!m_sampler.valid())
		return false;

	// Per-sprite dynamic uniform: one aligned slot per sprite, written once (static layout).
	const size_t align = device.minUniformBufferOffsetAlignment();
	m_stride = ((sizeof(SpriteData) + align - 1) / align) * align;
	std::vector<uint8_t> scratch(m_stride * m_spriteCount, 0);
	for (uint32_t i = 0; i < m_spriteCount; ++i)
		std::memcpy(scratch.data() + i * m_stride, &kSprites[i], sizeof(SpriteData));
	m_spriteBuffer = device.createBuffer(scratch.size(), acm::BufferUsage::Uniform);
	if (!m_spriteBuffer.valid())
		return false;
	m_spriteBuffer.write(scratch.data(), scratch.size());

	// Binding 0: per-sprite dynamic uniform (vertex). Binding 1: a 3-element sampler array.
	m_layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBufferDynamic, acm::ShaderStage::Vertex},
		{1, acm::DescriptorType::CombinedImageSampler, acm::ShaderStage::Fragment, kTextureCount},
	});
	if (!m_layout.valid())
		return false;
	m_descriptor = device.createDescriptorSet(m_layout);
	if (!m_descriptor.valid())
		return false;
	m_descriptor.setDynamicBuffer(0, m_spriteBuffer, sizeof(SpriteData));
	for (uint32_t i = 0; i < kTextureCount; ++i)
		m_descriptor.setTexture(1, m_textures[i], m_sampler, i);

	acm::PipelineConfig config;
	config.vertex = tb::loadShader(device, "sprite.vert.spv");
	config.fragment = tb::loadShader(device, "sprite.frag.spv");
	config.target = m_views[0]->renderTarget();
	config.descriptorLayout = m_layout;
	config.topology = acm::Topology::TriangleStrip;
	config.blend = acm::BlendMode::AlphaBlend;
	m_pipeline = device.createPipeline(config);
	return m_pipeline.valid();
}

void SpriteAtlasExample::onUpdate(acm::Device&, float)
{
	// Sprites are static (written once in onInit), so nothing changes per frame.
}

void SpriteAtlasExample::onRenderView(uint32_t viewIndex, float)
{
	m_views[viewIndex]->renderer().render([&](acm::CommandBuffer& cmd, uint32_t)
										  {
											  cmd.bindPipeline(m_pipeline);
											  for (uint32_t i = 0; i < m_spriteCount; ++i)
											  {
												  cmd.bindDescriptorSet(m_pipeline, m_descriptor, uint32_t(i * m_stride));
												  cmd.draw(4); // triangle strip quad
											  } });
}

void SpriteAtlasExample::onShutdown()
{
	m_pipeline.reset();
	m_descriptor.reset();
	m_layout.reset();
	m_spriteBuffer.reset();
	m_sampler.reset();
	for (auto& t : m_textures)
		t.reset();
}
