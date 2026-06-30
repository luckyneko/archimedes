#include "archimedes/acmDescriptorSet.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/acmSampler.h"
#include "archimedes/acmTexture.h"
#include "archimedes/nativeAPI.h"

#include <utility>

acm::DescriptorSet::DescriptorSet() = default;

acm::DescriptorSet::DescriptorSet(acm::ResourceRef<acm::native::DescriptorSet> resource)
	: m_resource(std::move(resource))
{
}

acm::DescriptorSet::DescriptorSet(acm::Error error)
	: m_error(std::move(error))
{
}

acm::DescriptorSet::DescriptorSet(const acm::DescriptorSet& other) = default;

acm::DescriptorSet& acm::DescriptorSet::operator=(const acm::DescriptorSet& other) = default;

acm::DescriptorSet::DescriptorSet(acm::DescriptorSet&& other) noexcept = default;

acm::DescriptorSet& acm::DescriptorSet::operator=(acm::DescriptorSet&& other) noexcept = default;

acm::DescriptorSet::~DescriptorSet() = default;

void acm::DescriptorSet::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::DescriptorSet::valid() const
{
	return m_resource.valid();
}

acm::Error acm::DescriptorSet::error() const
{
	return m_error;
}

acm::native::DescriptorSet* acm::DescriptorSet::native() const
{
	return m_resource.access();
}

void acm::DescriptorSet::setTexture(uint32_t binding, const acm::Texture& texture, const acm::Sampler& sampler, uint32_t arrayElement)
{
	if (auto* resource = m_resource.access())
		if (texture.native() && sampler.native())
			resource->setTexture(binding, *texture.native(), *sampler.native(), arrayElement);
}

void acm::DescriptorSet::setBuffer(uint32_t binding, const acm::Buffer& buffer, uint32_t arrayElement)
{
	if (auto* resource = m_resource.access())
		if (buffer.native())
			resource->setBuffer(binding, *buffer.native(), arrayElement);
}

void acm::DescriptorSet::setDynamicBuffer(uint32_t binding, const acm::Buffer& buffer, size_t elementSize, uint32_t arrayElement)
{
	if (auto* resource = m_resource.access())
		if (buffer.native())
			resource->setDynamicBuffer(binding, *buffer.native(), elementSize, arrayElement);
}

void acm::DescriptorSet::setStorageImage(uint32_t binding, const acm::Texture& texture, uint32_t arrayElement)
{
	if (auto* resource = m_resource.access())
		if (texture.native())
			resource->setStorageImage(binding, *texture.native(), arrayElement);
}
