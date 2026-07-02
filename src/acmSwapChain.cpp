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

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	SwapChain::SwapChain() = default;

	SwapChain::SwapChain(const SwapChain& other) = default;

	SwapChain& SwapChain::operator=(const SwapChain& other) = default;

	SwapChain::SwapChain(SwapChain&& other) noexcept = default;

	SwapChain& SwapChain::operator=(SwapChain&& other) noexcept = default;

	SwapChain::~SwapChain() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void SwapChain::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool SwapChain::valid() const
	{
		return m_resource.valid();
	}

	Error SwapChain::error() const
	{
		return m_error;
	}

	native::SwapChain* SwapChain::native() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Images
	// -----------------------------------------------------------------------------

	bool SwapChain::recreate()
	{
		if (auto* resource = m_resource.access())
			return resource->recreate();
		return false;
	}

	SurfaceFormat SwapChain::format() const
	{
		if (auto* resource = m_resource.access())
			return resource->format();
		return SurfaceFormat{};
	}

	Extent2D SwapChain::extent() const
	{
		if (auto* resource = m_resource.access())
			return resource->extent();
		return Extent2D{};
	}

	size_t SwapChain::renderTargetCount() const
	{
		if (auto* resource = m_resource.access())
			return resource->renderTargetCount();
		return 0;
	}

	RenderTarget SwapChain::renderTarget(size_t index) const
	{
		if (auto* resource = m_resource.access())
			return resource->renderTarget(index);
		return RenderTarget{};
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	SwapChain::SwapChain(ResourceRef<native::SwapChain> resource)
		: m_resource(std::move(resource))
	{
	}

	SwapChain::SwapChain(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
