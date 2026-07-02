/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <archimedes/archimedes.h>

#include <cstdint>

// The shared, centralised model both windows render from their own viewpoints: a
// rippling grid mesh. Its vertices live in a storage buffer (SSBO) that a **compute
// pipeline** rewrites every frame in update() — the main thread just dispatches it —
// so the geometry genuinely changes per frame; each window's vertex shader pulls from
// the same buffer via gl_VertexIndex. The index buffer, texture, and sampler are static
// and shared read-only.
class Scene
{
public:
	Scene() {}
	// `computeShader` is the mesh-deform compute module (mesh.comp.spv), loaded by main.
	bool init(acm::Device& device, acm::Shader computeShader);

	// Rewrite the mesh for animation time `t`: write the time uniform, then dispatch the
	// compute shader (synchronously — submitSync waits the queue idle). Must run only
	// while no render thread is reading the SSBO; the fork-join waits the device idle
	// first, so the prior frame's reads are already done.
	void update(float t);

	// Release all GPU resources before the borrowed device is destroyed.
	void shutdown();

	acm::Buffer vertexBuffer() const { return m_vertexBuffer; }
	acm::Buffer indexBuffer() const { return m_indexBuffer; }
	uint32_t indexCount() const { return m_indexCount; }
	acm::Texture texture() const { return m_texture; }
	acm::Sampler sampler() const { return m_sampler; }

private:
	acm::Device* m_device{nullptr};
	acm::Buffer m_vertexBuffer; // storage SSBO, rewritten every frame by compute
	acm::Buffer m_indexBuffer;	// static grid topology
	acm::Texture m_texture;
	acm::Sampler m_sampler;
	uint32_t m_indexCount{0};

	// Compute deform: pipeline + its descriptor set (SSBO at 0, time uniform at 1).
	acm::ComputePipeline m_computePipeline;
	acm::DescriptorSetLayout m_computeLayout;
	acm::DescriptorSet m_computeDescriptor;
	acm::Buffer m_paramsBuffer; // host-visible uniform: the animation time
};
