#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"
#include <cstdint>
#include <vector>

namespace acm
{
	// Describes the resources a shader expects in a descriptor set (set 0): a list
	// of DescriptorBindings, each a binding index + type + stage. Both Pipeline (its
	// pipeline layout) and DescriptorSet (allocation + writes) are built from one.
	// The sampler-count convenience form builds N combined-image-sampler bindings at
	// indices 0..n-1, all fragment-stage (the common texture-sampling case).
	class DescriptorSetLayout
	{
	public:
		DescriptorSetLayout() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		const std::vector<acm::DescriptorBinding>& bindings() const;
		VkDescriptorSetLayout vkDescriptorSetLayout() const;

	private:
		friend class Device; // only Device::createDescriptorSetLayout builds one
		DescriptorSetLayout(acm::Device device, const std::vector<acm::DescriptorBinding>& bindings);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
