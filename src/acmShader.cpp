/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmShader.h"

#include "archimedes/nativeAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Shader::Shader() = default;

	Shader::Shader(const Shader& other) = default;

	Shader& Shader::operator=(const Shader& other) = default;

	Shader::Shader(Shader&& other) noexcept = default;

	Shader& Shader::operator=(Shader&& other) noexcept = default;

	Shader::~Shader() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Shader::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool Shader::valid() const
	{
		return m_resource.valid();
	}

	Error Shader::error() const
	{
		return m_error;
	}

	native::Shader* Shader::native() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	Shader::Shader(ResourceRef<native::Shader> resource)
		: m_resource(std::move(resource))
	{
	}

	Shader::Shader(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
