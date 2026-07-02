/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmRenderTarget.h"

#include "archimedes/backendAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	RenderTarget::RenderTarget() = default;

	RenderTarget::RenderTarget(const RenderTarget& other) = default;

	RenderTarget& RenderTarget::operator=(const RenderTarget& other) = default;

	RenderTarget::RenderTarget(RenderTarget&& other) noexcept = default;

	RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept = default;

	RenderTarget::~RenderTarget() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void RenderTarget::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool RenderTarget::valid() const
	{
		return m_resource.valid();
	}

	Error RenderTarget::error() const
	{
		return m_error;
	}

	backend::RenderTarget* RenderTarget::backend() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Properties
	// -----------------------------------------------------------------------------

	Extent2D RenderTarget::extent() const
	{
		if (auto* resource = m_resource.access())
			return resource->extent();
		return Extent2D{};
	}

	bool RenderTarget::hasDepth() const
	{
		if (auto* resource = m_resource.access())
			return resource->hasDepth();
		return false;
	}

	bool RenderTarget::isMultisampled() const
	{
		if (auto* resource = m_resource.access())
			return resource->multisampled();
		return false;
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	RenderTarget::RenderTarget(ResourceRef<backend::RenderTarget> resource)
		: m_resource(std::move(resource))
	{
	}

	RenderTarget::RenderTarget(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
