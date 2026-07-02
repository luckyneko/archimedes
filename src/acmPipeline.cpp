/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmPipeline.h"

#include "archimedes/nativeAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Pipeline::Pipeline() = default;

	Pipeline::Pipeline(const Pipeline& other) = default;

	Pipeline& Pipeline::operator=(const Pipeline& other) = default;

	Pipeline::Pipeline(Pipeline&& other) noexcept = default;

	Pipeline& Pipeline::operator=(Pipeline&& other) noexcept = default;

	Pipeline::~Pipeline() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Pipeline::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool Pipeline::valid() const
	{
		return m_resource.valid();
	}

	Error Pipeline::error() const
	{
		return m_error;
	}

	native::Pipeline* Pipeline::native() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	Pipeline::Pipeline(ResourceRef<native::Pipeline> resource)
		: m_resource(std::move(resource))
	{
	}

	Pipeline::Pipeline(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
