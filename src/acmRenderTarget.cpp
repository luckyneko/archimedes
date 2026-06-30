#include "archimedes/acmRenderTarget.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::RenderTarget::RenderTarget() = default;

acm::RenderTarget::RenderTarget(acm::ResourceRef<acm::native::RenderTarget> resource)
	: m_resource(std::move(resource))
{
}

acm::RenderTarget::RenderTarget(acm::Error error)
	: m_error(std::move(error))
{
}

acm::RenderTarget::RenderTarget(const acm::RenderTarget& other) = default;

acm::RenderTarget& acm::RenderTarget::operator=(const acm::RenderTarget& other) = default;

acm::RenderTarget::RenderTarget(acm::RenderTarget&& other) noexcept = default;

acm::RenderTarget& acm::RenderTarget::operator=(acm::RenderTarget&& other) noexcept = default;

acm::RenderTarget::~RenderTarget() = default;

void acm::RenderTarget::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::RenderTarget::valid() const
{
	return m_resource.valid();
}

acm::Error acm::RenderTarget::error() const
{
	return m_error;
}

acm::native::RenderTarget* acm::RenderTarget::native() const
{
	return m_resource.access();
}

acm::Extent2D acm::RenderTarget::getExtent() const
{
	if (auto* resource = m_resource.access())
		return resource->extent();
	return acm::Extent2D{};
}

bool acm::RenderTarget::hasDepth() const
{
	if (auto* resource = m_resource.access())
		return resource->hasDepth();
	return false;
}

bool acm::RenderTarget::isMultisampled() const
{
	if (auto* resource = m_resource.access())
		return resource->multisampled();
	return false;
}
