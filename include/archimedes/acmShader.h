/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"
#include "archimedes/acmBackend.h"

#include <vector>

namespace acm
{
	// Copyable handle to a shader module created from SPIR-V bytecode.
	class Shader
	{
	public:
		// Lifetime
		Shader();
		Shader(const acm::Shader& other);
		Shader& operator=(const acm::Shader& other);
		Shader(acm::Shader&& other) noexcept;
		Shader& operator=(acm::Shader&& other) noexcept;
		~Shader();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::backend::Shader* backend() const;

	private:
		// Construction
		friend acm::backend::Device;
		Shader(acm::ResourceRef<acm::backend::Shader> resource);
		explicit Shader(acm::Error error);

		acm::ResourceRef<acm::backend::Shader> m_resource;
		acm::Error m_error;
	};
} // namespace acm
