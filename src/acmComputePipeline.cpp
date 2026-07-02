/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmComputePipeline.h"

#include "archimedes/backendAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	ComputePipeline::ComputePipeline() = default;

	ComputePipeline::ComputePipeline(const ComputePipeline& other) = default;

	ComputePipeline& ComputePipeline::operator=(const ComputePipeline& other) = default;

	ComputePipeline::ComputePipeline(ComputePipeline&& other) noexcept = default;

	ComputePipeline& ComputePipeline::operator=(ComputePipeline&& other) noexcept = default;

	ComputePipeline::~ComputePipeline() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void ComputePipeline::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool ComputePipeline::valid() const
	{
		return m_resource.valid();
	}

	Error ComputePipeline::error() const
	{
		return m_error;
	}

	backend::ComputePipeline* ComputePipeline::backend() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	ComputePipeline::ComputePipeline(ResourceRef<backend::ComputePipeline> resource)
		: m_resource(std::move(resource))
	{
	}

	ComputePipeline::ComputePipeline(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
