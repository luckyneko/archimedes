/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmSwapChain.h"

#include "archimedes/acmRenderTarget.h"
#include "archimedes/nativeAPI.h"

#include <utility>

acm::SwapChain::SwapChain() = default;

acm::SwapChain::SwapChain(acm::ResourceRef<acm::native::SwapChain> resource)
	: m_resource(std::move(resource))
{
}

acm::SwapChain::SwapChain(acm::Error error)
	: m_error(std::move(error))
{
}

acm::SwapChain::SwapChain(const acm::SwapChain& other) = default;

acm::SwapChain& acm::SwapChain::operator=(const acm::SwapChain& other) = default;

acm::SwapChain::SwapChain(acm::SwapChain&& other) noexcept = default;

acm::SwapChain& acm::SwapChain::operator=(acm::SwapChain&& other) noexcept = default;

acm::SwapChain::~SwapChain() = default;

void acm::SwapChain::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::SwapChain::valid() const
{
	return m_resource.valid();
}

acm::Error acm::SwapChain::error() const
{
	return m_error;
}

acm::native::SwapChain* acm::SwapChain::native() const
{
	return m_resource.access();
}

bool acm::SwapChain::recreate()
{
	if (auto* resource = m_resource.access())
		return resource->recreate();
	return false;
}

acm::SurfaceFormat acm::SwapChain::getFormat() const
{
	if (auto* resource = m_resource.access())
		return resource->format();
	return acm::SurfaceFormat{};
}

acm::Extent2D acm::SwapChain::getExtents() const
{
	if (auto* resource = m_resource.access())
		return resource->extent();
	return acm::Extent2D{};
}

size_t acm::SwapChain::getRenderTargetCount() const
{
	if (auto* resource = m_resource.access())
		return resource->renderTargetCount();
	return 0;
}

acm::RenderTarget acm::SwapChain::getRenderTarget(size_t idx) const
{
	if (auto* resource = m_resource.access())
		return resource->renderTarget(idx);
	return acm::RenderTarget{};
}
