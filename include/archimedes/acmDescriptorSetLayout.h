#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"
#include "archimedes/acmNative.h"
#include "archimedes/acmTypes.h"

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
		DescriptorSetLayout();
		DescriptorSetLayout(const acm::DescriptorSetLayout& other);
		DescriptorSetLayout& operator=(const acm::DescriptorSetLayout& other);
		DescriptorSetLayout(acm::DescriptorSetLayout&& other) noexcept;
		DescriptorSetLayout& operator=(acm::DescriptorSetLayout&& other) noexcept;
		~DescriptorSetLayout();

		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::native::DescriptorSetLayout* native() const;

	private:
		friend acm::native::Device;
		DescriptorSetLayout(acm::ResourceRef<acm::native::DescriptorSetLayout> resource);
		explicit DescriptorSetLayout(acm::Error error);

		acm::ResourceRef<acm::native::DescriptorSetLayout> m_resource;
		acm::Error m_error;
	};
} // namespace acm
