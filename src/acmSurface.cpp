/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmSurface.h"

#include "archimedes/backendAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Surface::Surface() = default;

	Surface::Surface(const Surface& other) = default;

	Surface& Surface::operator=(const Surface& other) = default;

	Surface::Surface(Surface&& other) noexcept = default;

	Surface& Surface::operator=(Surface&& other) noexcept = default;

	Surface::~Surface() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Surface::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool Surface::valid() const
	{
		return m_resource.valid();
	}

	Error Surface::error() const
	{
		return m_error;
	}

	backend::Surface* Surface::backend() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Capabilities
	// -----------------------------------------------------------------------------

	const std::vector<GPUSurfaceSupport>& Surface::getGPUSupport() const
	{
		static const std::vector<GPUSurfaceSupport> empty;
		if (auto* resource = m_resource.access())
			return resource->support();
		return empty;
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	Surface::Surface(ResourceRef<backend::Surface> resource)
		: m_resource(std::move(resource))
	{
	}

	Surface::Surface(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
