/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "Scene.h"

#include <vector>

namespace
{
	// Grid resolution (vertices per side). Must match mesh.comp's kGrid. 64x64 = 4096
	// verts, deformed by the compute shader as 8x8 workgroups of 8x8 invocations.
	constexpr uint32_t kGrid = 64;
	constexpr uint32_t kLocalSize = 8; // mesh.comp local_size_x/y

	// std430 layout matching mesh.vert / mesh.comp's `struct Vertex { vec4 pos; vec4
	// normal; vec4 uv; }` — three 16-byte-aligned vec4s, 48 bytes total. Used here only
	// to size the SSBO; the compute shader fills it.
	struct GpuVertex
	{
		float pos[4];
		float normal[4];
		float uv[4];
	};

	// Grid triangle topology: two triangles per cell, (kGrid-1)^2 cells.
	std::vector<uint32_t> gridIndices()
	{
		std::vector<uint32_t> indices;
		indices.reserve(size_t(kGrid - 1) * (kGrid - 1) * 6);
		for (uint32_t gz = 0; gz + 1 < kGrid; ++gz)
			for (uint32_t gx = 0; gx + 1 < kGrid; ++gx)
			{
				const uint32_t a = gz * kGrid + gx;
				const uint32_t b = a + 1;
				const uint32_t c = a + kGrid;
				const uint32_t d = c + 1;
				indices.insert(indices.end(), {a, c, b, b, c, d});
			}
		return indices;
	}

	// A procedural checkerboard so the texturing (and mipmaps / anisotropy on the
	// rippling, grazing surface) is obvious. B8G8R8A8_Unorm memory order is [B,G,R,A].
	std::vector<uint8_t> checkerboard(uint32_t size, uint32_t cells)
	{
		std::vector<uint8_t> px(size_t(size) * size * 4);
		const uint32_t cell = size / cells;
		for (uint32_t y = 0; y < size; ++y)
			for (uint32_t x = 0; x < size; ++x)
			{
				const bool even = ((x / cell) + (y / cell)) % 2 == 0;
				uint8_t* p = &px[(size_t(y) * size + x) * 4];
				// [B,G,R,A]: light gray vs. teal.
				p[0] = even ? 225 : 150;
				p[1] = even ? 225 : 110;
				p[2] = even ? 225 : 40;
				p[3] = 255;
			}
		return px;
	}
} // namespace

bool Scene::init(acm::Device& device, acm::Shader computeShader)
{
	m_device = &device;

	// The mesh vertices: a storage buffer the compute shader rewrites each frame and the
	// vertex shaders read as an SSBO.
	const size_t vertexBytes = size_t(kGrid) * kGrid * sizeof(GpuVertex);
	m_vertexBuffer = device.createBuffer(vertexBytes, acm::BufferUsage::Storage);

	// Static grid topology in a device-local index buffer.
	const std::vector<uint32_t> indices = gridIndices();
	m_indexBuffer = device.createBuffer(indices.size() * sizeof(uint32_t), acm::BufferUsage::Index);
	if (!m_vertexBuffer.valid() || !m_indexBuffer.valid())
		return false;
	m_indexBuffer.write(indices.data(), indices.size() * sizeof(uint32_t));
	m_indexCount = uint32_t(indices.size());

	// Shared mipmapped checkerboard, sampled anisotropically.
	const std::vector<uint8_t> pixels = checkerboard(256, 8);
	m_texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, acm::Extent2D{256, 256}, acm::TextureConfig{true});
	if (!m_texture.valid())
		return false;
	m_texture.upload(pixels.data(), pixels.size());
	m_sampler = device.createSampler(16.0f); // anisotropic if supported

	// Compute deform pipeline: writes the SSBO (binding 0) from a time uniform (binding 1).
	m_paramsBuffer = device.createBuffer(sizeof(float) * 4, acm::BufferUsage::Uniform);
	m_computeLayout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::StorageBuffer, acm::ShaderStage::Compute},
		{1, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Compute},
	});
	if (!m_paramsBuffer.valid() || !m_computeLayout.valid() || !computeShader.valid())
		return false;
	m_computeDescriptor = device.createDescriptorSet(m_computeLayout);
	m_computePipeline = device.createComputePipeline(computeShader, m_computeLayout);
	if (!m_computeDescriptor.valid() || !m_computePipeline.valid())
		return false;
	m_computeDescriptor.setBuffer(0, m_vertexBuffer);
	m_computeDescriptor.setBuffer(1, m_paramsBuffer);

	update(0.0f); // seed the SSBO so the first frame has geometry
	return true;
}

void Scene::update(float t)
{
	if (!m_computePipeline.valid())
		return;

	const float params[4] = {t, 0.0f, 0.0f, 0.0f};
	m_paramsBuffer.write(params, sizeof(params));

	const uint32_t groups = (kGrid + kLocalSize - 1) / kLocalSize;
	m_device->submitSync([this, groups](acm::CommandBuffer& cmd)
						 {
							// Bracket the dispatch with barriers so the shared SSBO is synced
							// purely by queue-submission-order dependencies (not a wait-idle):
							// wait for the previous frame's vertex reads before overwriting it
							// (write-after-read), and make this frame's writes visible to the
							// next renders' vertex reads (read-after-write).
							cmd.bufferBarrier(m_vertexBuffer, acm::ShaderStage::Vertex, acm::ShaderStage::Compute);
							cmd.bindComputePipeline(m_computePipeline);
							cmd.bindComputeDescriptorSet(m_computePipeline, m_computeDescriptor);
							cmd.dispatch(groups, groups, 1);
							cmd.bufferBarrier(m_vertexBuffer, acm::ShaderStage::Compute, acm::ShaderStage::Vertex); });
}

void Scene::shutdown()
{
	m_computePipeline.reset();
	m_computeDescriptor.reset();
	m_computeLayout.reset();
	m_paramsBuffer.reset();
	m_vertexBuffer.reset();
	m_indexBuffer.reset();
	m_texture.reset();
	m_sampler.reset();
	m_device = nullptr;
}
