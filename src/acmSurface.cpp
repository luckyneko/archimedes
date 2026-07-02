/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmSurface.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Surface::Surface() = default;

acm::Surface::Surface(acm::ResourceRef<acm::native::Surface> resource)
	: m_resource(std::move(resource))
{
}

acm::Surface::Surface(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Surface::Surface(const acm::Surface& other) = default;

acm::Surface& acm::Surface::operator=(const acm::Surface& other) = default;

acm::Surface::Surface(acm::Surface&& other) noexcept = default;

acm::Surface& acm::Surface::operator=(acm::Surface&& other) noexcept = default;

acm::Surface::~Surface() = default;

void acm::Surface::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::Surface::valid() const
{
	return m_resource.valid();
}

acm::Error acm::Surface::error() const
{
	return m_error;
}

acm::native::Surface* acm::Surface::native() const
{
	return m_resource.access();
}

const std::vector<acm::GPUSurfaceSupport>& acm::Surface::getGPUSupport() const
{
	static const std::vector<acm::GPUSurfaceSupport> empty;
	if (auto* resource = m_resource.access())
		return resource->support();
	return empty;
}
