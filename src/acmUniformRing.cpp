#include "archimedes/acmUniformRing.h"
#include "archimedes/acmBuffer.h"
#include "archimedes/acmDescriptorSet.h"
#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmDevice.h"
#include <cassert>
#include <vector>

// Composes other acm handles (a layout + a buffer/set per frame); each of those owns
// its own Vulkan teardown, so the ring's impl needs no destructor of its own.
struct acm::UniformRing::impl
{
	uint32_t binding{0};
	acm::DescriptorSetLayout layout;
	std::vector<acm::Buffer> buffers;	  // one per frame
	std::vector<acm::DescriptorSet> sets; // one per frame, pointing at the matching buffer
};

acm::UniformRing::UniformRing(acm::Device device, size_t bytes, uint32_t binding, acm::ShaderStage stage, uint32_t frames)
	: m()
{
	assert(bytes > 0 && frames > 0 && "acm::UniformRing: zero size/frames");

	auto impl = std::make_shared<acm::UniformRing::impl>();
	impl->binding = binding;
	impl->layout = device.createDescriptorSetLayout({{binding, acm::DescriptorType::UniformBuffer, stage}});
	if (!impl->layout.valid())
	{
		m_error = impl->layout.error();
		return;
	}

	impl->buffers.reserve(frames);
	impl->sets.reserve(frames);
	for (uint32_t i = 0; i < frames; ++i)
	{
		acm::Buffer buffer = device.createBuffer(bytes, acm::BufferUsage::Uniform);
		acm::DescriptorSet set = device.createDescriptorSet(impl->layout);
		if (!buffer.valid() || !set.valid())
		{
			m_error = !buffer.valid() ? buffer.error() : set.error();
			return;
		}
		set.setBuffer(binding, buffer);
		impl->buffers.push_back(buffer);
		impl->sets.push_back(set);
	}

	m = impl;
}

uint32_t acm::UniformRing::frames() const
{
	return uint32_t(m->buffers.size());
}

acm::DescriptorSetLayout acm::UniformRing::descriptorLayout() const
{
	return m->layout;
}

acm::DescriptorSet acm::UniformRing::descriptorSet(uint32_t frame) const
{
	return m->sets[frame];
}

void acm::UniformRing::update(uint32_t frame, const void* data, size_t size)
{
	m->buffers[frame].write(data, size);
}
