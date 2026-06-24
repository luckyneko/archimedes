#include "archimedes/acmDescriptorSet.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/acmSampler.h"
#include "archimedes/acmTexture.h"
#include "archimedes/nativeAPI.h"

#include <utility>

acm::DescriptorSet::DescriptorSet() = default;

acm::DescriptorSet::DescriptorSet(acm::native::DescriptorSet* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::DescriptorSet::DescriptorSet(acm::Error error)
	: m_error(std::move(error))
{
}

acm::DescriptorSet::DescriptorSet(const acm::DescriptorSet& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::DescriptorSet& acm::DescriptorSet::operator=(const acm::DescriptorSet& other)
{
	if (this == &other)
		return *this;
	reset();
	m_resource = other.m_resource;
	m_handle = other.m_handle;
	m_error = other.m_error;
	if (m_handle.valid())
		m_resource->retain(m_handle);
	return *this;
}

acm::DescriptorSet::DescriptorSet(acm::DescriptorSet&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::DescriptorSet& acm::DescriptorSet::operator=(acm::DescriptorSet&& other) noexcept
{
	if (this == &other)
		return *this;
	reset();
	m_resource = other.m_resource;
	m_handle = other.m_handle;
	m_error = std::move(other.m_error);
	other.m_resource = nullptr;
	other.m_handle.reset();
	return *this;
}

acm::DescriptorSet::~DescriptorSet()
{
	reset();
}

void acm::DescriptorSet::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::DescriptorSet::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::DescriptorSet::error() const
{
	return m_error;
}

void acm::DescriptorSet::setTexture(uint32_t binding, const acm::Texture& texture, const acm::Sampler& sampler, uint32_t arrayElement)
{
	if (m_resource && texture.native() && sampler.native())
		m_resource->setTexture(m_handle, binding, *texture.native(), texture.handle(), *sampler.native(), sampler.handle(), arrayElement);
}

void acm::DescriptorSet::setBuffer(uint32_t binding, const acm::Buffer& buffer, uint32_t arrayElement)
{
	if (m_resource && buffer.native())
		m_resource->setBuffer(m_handle, binding, *buffer.native(), buffer.handle(), arrayElement);
}

void acm::DescriptorSet::setDynamicBuffer(uint32_t binding, const acm::Buffer& buffer, size_t elementSize, uint32_t arrayElement)
{
	if (m_resource && buffer.native())
		m_resource->setDynamicBuffer(m_handle, binding, *buffer.native(), buffer.handle(), elementSize, arrayElement);
}

void acm::DescriptorSet::setStorageImage(uint32_t binding, const acm::Texture& texture, uint32_t arrayElement)
{
	if (m_resource && texture.native())
		m_resource->setStorageImage(m_handle, binding, *texture.native(), texture.handle(), arrayElement);
}
