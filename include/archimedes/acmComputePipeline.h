#pragma once

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmShader.h"
#include "archimedes/acmVkFwd.h"

namespace acm
{
	// A compute pipeline: a single compute-stage shader + a pipeline layout. No render
	// pass, no fixed-function state — just `dispatch`. Built from a compute `Shader`
	// (any SPIR-V compute module) and an optional `DescriptorSetLayout` describing the
	// resources it reads/writes (typically a storage buffer + a small uniform). Record
	// `bindComputePipeline` -> `bindComputeDescriptorSet` -> `dispatch` on a
	// CommandBuffer, then submit (e.g. via Device::submitSync for one-shot work).
	class ComputePipeline
	{
	public:
		ComputePipeline() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		VkPipeline vkPipeline() const;
		VkPipelineLayout vkPipelineLayout() const;

	private:
		friend class Device; // only Device::createComputePipeline builds one
		ComputePipeline(acm::Device device, acm::Shader compute, acm::DescriptorSetLayout layout);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
