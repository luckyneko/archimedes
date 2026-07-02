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
#include "archimedes/acmNative.h"
#include "archimedes/acmTypes.h"

#include <cstddef>

namespace acm
{
	class SwapChain
	{
	public:
		SwapChain();
		SwapChain(const acm::SwapChain& other);
		SwapChain& operator=(const acm::SwapChain& other);
		SwapChain(acm::SwapChain&& other) noexcept;
		SwapChain& operator=(acm::SwapChain&& other) noexcept;
		~SwapChain();

		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::native::SwapChain* native() const;

		bool recreate();
		acm::SurfaceFormat getFormat() const;
		acm::Extent2D getExtents() const;
		size_t getRenderTargetCount() const;
		acm::RenderTarget getRenderTarget(size_t idx) const;

	private:
		friend acm::native::Device;
		SwapChain(acm::ResourceRef<acm::native::SwapChain> resource);
		explicit SwapChain(acm::Error error);

		acm::ResourceRef<acm::native::SwapChain> m_resource;
		acm::Error m_error;
	};
} // namespace acm
