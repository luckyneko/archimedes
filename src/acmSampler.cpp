/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmSampler.h"

#include "archimedes/nativeAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Sampler::Sampler() = default;

	Sampler::Sampler(const Sampler& other) = default;

	Sampler& Sampler::operator=(const Sampler& other) = default;

	Sampler::Sampler(Sampler&& other) noexcept = default;

	Sampler& Sampler::operator=(Sampler&& other) noexcept = default;

	Sampler::~Sampler() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Sampler::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool Sampler::valid() const
	{
		return m_resource.valid();
	}

	Error Sampler::error() const
	{
		return m_error;
	}

	native::Sampler* Sampler::native() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	Sampler::Sampler(ResourceRef<native::Sampler> resource)
		: m_resource(std::move(resource))
	{
	}

	Sampler::Sampler(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
