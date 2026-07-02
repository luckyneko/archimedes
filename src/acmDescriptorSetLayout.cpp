/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmDescriptorSetLayout.h"

#include "archimedes/backendAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	DescriptorSetLayout::DescriptorSetLayout() = default;

	DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetLayout& other) = default;

	DescriptorSetLayout& DescriptorSetLayout::operator=(const DescriptorSetLayout& other) = default;

	DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other) noexcept = default;

	DescriptorSetLayout& DescriptorSetLayout::operator=(DescriptorSetLayout&& other) noexcept = default;

	DescriptorSetLayout::~DescriptorSetLayout() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void DescriptorSetLayout::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool DescriptorSetLayout::valid() const
	{
		return m_resource.valid();
	}

	Error DescriptorSetLayout::error() const
	{
		return m_error;
	}

	backend::DescriptorSetLayout* DescriptorSetLayout::backend() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	DescriptorSetLayout::DescriptorSetLayout(ResourceRef<backend::DescriptorSetLayout> resource)
		: m_resource(std::move(resource))
	{
	}

	DescriptorSetLayout::DescriptorSetLayout(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
