#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
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
		const acm::Handle& handle() const { return m_handle; }
		acm::native::DescriptorSetLayout* native() const { return m_resource; }

	private:
		friend acm::native::Device;
		DescriptorSetLayout(acm::native::DescriptorSetLayout* resource, acm::Handle handle);
		explicit DescriptorSetLayout(acm::Error error);

		acm::native::DescriptorSetLayout* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
