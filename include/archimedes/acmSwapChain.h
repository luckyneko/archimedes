/*
 *  Created by LuckyNeko on 07/06/2026.
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
#include "archimedes/acmTypes.h"

#include <cstddef>

namespace acm
{
	// Copyable handle to a presentation swapchain. Recreate invalidates old per-image
	// RenderTarget handles and rebuilds them for the surface's current extent.
	class SwapChain
	{
	public:
		// Lifetime
		SwapChain();
		SwapChain(const acm::SwapChain& other);
		SwapChain& operator=(const acm::SwapChain& other);
		SwapChain(acm::SwapChain&& other) noexcept;
		SwapChain& operator=(acm::SwapChain&& other) noexcept;
		~SwapChain();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::backend::SwapChain* backend() const;

		// Images
		// Returns false for a zero-sized/minimized surface; callers should skip the
		// frame and retry once the surface has a drawable extent.
		bool recreate();
		acm::SurfaceFormat format() const;
		acm::Extent2D extent() const;
		size_t renderTargetCount() const;
		acm::RenderTarget renderTarget(size_t index) const;

	private:
		// Construction
		friend acm::backend::Device;
		SwapChain(acm::ResourceRef<acm::backend::SwapChain> resource);
		explicit SwapChain(acm::Error error);

		acm::ResourceRef<acm::backend::SwapChain> m_resource;
		acm::Error m_error;
	};
} // namespace acm
