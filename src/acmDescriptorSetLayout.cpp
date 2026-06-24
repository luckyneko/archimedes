#include "archimedes/acmDescriptorSetLayout.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::DescriptorSetLayout::DescriptorSetLayout() = default;

acm::DescriptorSetLayout::DescriptorSetLayout(acm::native::DescriptorSetLayout* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::DescriptorSetLayout::DescriptorSetLayout(acm::Error error)
	: m_error(std::move(error))
{
}

acm::DescriptorSetLayout::DescriptorSetLayout(const acm::DescriptorSetLayout& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::DescriptorSetLayout& acm::DescriptorSetLayout::operator=(const acm::DescriptorSetLayout& other)
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

acm::DescriptorSetLayout::DescriptorSetLayout(acm::DescriptorSetLayout&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::DescriptorSetLayout& acm::DescriptorSetLayout::operator=(acm::DescriptorSetLayout&& other) noexcept
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

acm::DescriptorSetLayout::~DescriptorSetLayout()
{
	reset();
}

void acm::DescriptorSetLayout::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::DescriptorSetLayout::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::DescriptorSetLayout::error() const
{
	return m_error;
}
