/*
 *  Created by LuckyNeko on 07/06/2026.
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
#include "archimedes/acmTypes.h"

namespace acm
{
	// Copyable handle to renderable attachment metadata. Swapchain targets borrow their
	// image; offscreen targets render into a Texture and leave it in the requested final
	// layout after endRendering().
	class RenderTarget
	{
	public:
		// Lifetime
		RenderTarget();
		RenderTarget(const acm::RenderTarget& other);
		RenderTarget& operator=(const acm::RenderTarget& other);
		RenderTarget(acm::RenderTarget&& other) noexcept;
		RenderTarget& operator=(acm::RenderTarget&& other) noexcept;
		~RenderTarget();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::backend::RenderTarget* backend() const;

		// Properties
		acm::Extent2D extent() const;
		bool hasDepth() const;
		bool isMultisampled() const;

	private:
		// Construction
		friend acm::backend::Device;
		RenderTarget(acm::ResourceRef<acm::backend::RenderTarget> resource);
		explicit RenderTarget(acm::Error error);

		acm::ResourceRef<acm::backend::RenderTarget> m_resource;
		acm::Error m_error;
	};
} // namespace acm
