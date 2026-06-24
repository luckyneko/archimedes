#include "archimedes/acmSurface.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Surface::Surface() = default;

acm::Surface::Surface(acm::native::Surface* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::Surface::Surface(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Surface::Surface(const acm::Surface& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::Surface& acm::Surface::operator=(const acm::Surface& other)
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

acm::Surface::Surface(acm::Surface&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::Surface& acm::Surface::operator=(acm::Surface&& other) noexcept
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

acm::Surface::~Surface()
{
	reset();
}

void acm::Surface::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::Surface::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::Surface::error() const
{
	return m_error;
}

const std::vector<acm::GPUSurfaceSupport>& acm::Surface::getGPUSupport() const
{
	static const std::vector<acm::GPUSurfaceSupport> empty;
	return m_resource ? m_resource->support(m_handle) : empty;
}
