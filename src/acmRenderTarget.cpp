#include "archimedes/acmRenderTarget.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::RenderTarget::RenderTarget() = default;

acm::RenderTarget::RenderTarget(acm::native::RenderTarget* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::RenderTarget::RenderTarget(acm::Error error)
	: m_error(std::move(error))
{
}

acm::RenderTarget::RenderTarget(const acm::RenderTarget& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::RenderTarget& acm::RenderTarget::operator=(const acm::RenderTarget& other)
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

acm::RenderTarget::RenderTarget(acm::RenderTarget&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::RenderTarget& acm::RenderTarget::operator=(acm::RenderTarget&& other) noexcept
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

acm::RenderTarget::~RenderTarget()
{
	reset();
}

void acm::RenderTarget::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::RenderTarget::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::RenderTarget::error() const
{
	return m_error;
}

acm::Extent2D acm::RenderTarget::getExtent() const
{
	return m_resource ? m_resource->extent(m_handle) : acm::Extent2D{};
}

bool acm::RenderTarget::hasDepth() const
{
	return m_resource && m_resource->hasDepth(m_handle);
}

bool acm::RenderTarget::isMultisampled() const
{
	return m_resource && m_resource->multisampled(m_handle);
}
