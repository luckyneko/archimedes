/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmBackend.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"

namespace acm
{
	// Owns a native sampler. Linear min/mag filtering, clamp-to-edge addressing, nearest
	// mipmap mode; optional anisotropic filtering (createSampler's maxAnisotropy > 1,
	// gated on the samplerAnisotropy device feature and clamped to its limit). One
	// sampler is reusable across many textures/descriptor sets.
	class Sampler
	{
	public:
		// Lifetime
		Sampler();
		Sampler(const acm::Sampler& other);
		Sampler& operator=(const acm::Sampler& other);
		Sampler(acm::Sampler&& other) noexcept;
		Sampler& operator=(acm::Sampler&& other) noexcept;
		~Sampler();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::backend::Sampler* backend() const;

	private:
		// Construction
		friend acm::backend::Device;
		Sampler(acm::ResourceRef<acm::backend::Sampler> resource);
		explicit Sampler(acm::Error error);

		acm::ResourceRef<acm::backend::Sampler> m_resource;
		acm::Error m_error;
	};
} // namespace acm
