#include "archimedes/acmSwapChain.h"

#include "archimedes/acmRenderTarget.h"
#include "archimedes/nativeAPI.h"

#include <utility>

acm::SwapChain::SwapChain() = default;

acm::SwapChain::SwapChain(acm::native::SwapChain* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::SwapChain::SwapChain(acm::Error error)
	: m_error(std::move(error))
{
}

acm::SwapChain::SwapChain(const acm::SwapChain& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::SwapChain& acm::SwapChain::operator=(const acm::SwapChain& other)
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

acm::SwapChain::SwapChain(acm::SwapChain&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::SwapChain& acm::SwapChain::operator=(acm::SwapChain&& other) noexcept
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

acm::SwapChain::~SwapChain()
{
	reset();
}

void acm::SwapChain::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::SwapChain::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::SwapChain::error() const
{
	return m_error;
}

bool acm::SwapChain::recreate()
{
	return m_resource && m_resource->recreate(m_handle);
}

acm::SurfaceFormat acm::SwapChain::getFormat() const
{
	return m_resource ? m_resource->format(m_handle) : acm::SurfaceFormat{};
}

acm::Extent2D acm::SwapChain::getExtents() const
{
	return m_resource ? m_resource->extent(m_handle) : acm::Extent2D{};
}

size_t acm::SwapChain::getRenderTargetCount() const
{
	return m_resource ? m_resource->renderTargetCount(m_handle) : 0;
}

acm::RenderTarget acm::SwapChain::getRenderTarget(size_t idx) const
{
	return m_resource ? m_resource->renderTarget(m_handle, idx) : acm::RenderTarget{};
}
