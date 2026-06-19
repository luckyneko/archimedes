#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmTypes.h"
#include <cstddef>
#include <cstdint>
#include <memory>

namespace acm
{
	// A per-frame-in-flight ring for a single uniform: N host-visible uniform buffers
	// behind N one-binding descriptor sets (sharing one layout), so a uniform that
	// changes every frame can be rewritten for the current frame without racing a
	// still-in-flight one. Size N to acm::Renderer::MaxFramesInFlight (the default)
	// and index by the frame slot the renderer hands the record callback:
	//
	//   acm::UniformRing ring = device.createUniformRing(sizeof(MVP)); // binding 0, vertex
	//   config.descriptorLayout = ring.descriptorLayout();             // build the pipeline with it
	//   ...
	//   ring.update(frame, &mvp, sizeof(mvp));
	//   cmd.bindDescriptorSet(pipeline, ring.descriptorSet(frame));
	//
	// This bundles the common single-uniform case; for a set with several bindings
	// (e.g. a uniform + a sampler) drive the Buffer / DescriptorSet directly instead.
	class UniformRing
	{
	public:
		UniformRing() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		uint32_t frames() const;
		acm::DescriptorSetLayout descriptorLayout() const; // pass to PipelineConfig::descriptorLayout
		acm::DescriptorSet descriptorSet(uint32_t frame) const;

		// Writes `data` into frame `frame`'s buffer (clamped to the ring's byte size).
		// Safe for the slot the renderer just handed you — its fence has been waited.
		void update(uint32_t frame, const void* data, size_t size);

	private:
		friend class Device; // only Device::createUniformRing builds one
		UniformRing(acm::Device device, size_t bytes, uint32_t binding, acm::ShaderStage stage, uint32_t frames);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
