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
#include "archimedes/acmNative.h"
#include "archimedes/acmTypes.h"

#include <cstddef>
#include <cstdint>

namespace acm
{
	// An owned native 2D image with device-local memory and an image view. Unlike the
	// swapchain's borrowed images, a Texture allocates and destroys its own GPU memory.
	// A color format gives usage COLOR_ATTACHMENT | SAMPLED | TRANSFER_SRC | TRANSFER_DST
	// (rendered into, sampled, copied out, and uploaded into — the render-to-texture and
	// CPU-upload paths); a depth format (D32_Sfloat / D24_Unorm_S8_Uint) instead gives
	// DEPTH_STENCIL_ATTACHMENT with a depth-aspect view, for use as a RenderTarget's
	// depth buffer. `mipmapped` (color only) allocates a full mip chain that upload()
	// fills — note the view then spans all levels, so a mipmapped texture is a sampling
	// resource, not a RenderTarget attachment (which needs a single-level view).
	class Texture
	{
	public:
		// Lifetime
		Texture();
		Texture(const acm::Texture& other);
		Texture& operator=(const acm::Texture& other);
		Texture(acm::Texture&& other) noexcept;
		Texture& operator=(acm::Texture&& other) noexcept;
		~Texture();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::native::Texture* native() const;

		// Upload
		// Uploads CPU pixels (tightly packed, matching the texture's format/extent) via
		// a staging buffer + one-shot copy. For a mipmapped texture it then generates
		// the rest of the chain by blitting; leaves all levels in SHADER_READ_ONLY
		// layout — i.e. ready to sample. Color textures only; synchronous (load-time).
		acm::Error upload(const void* pixels, size_t size);

		// Properties
		acm::Format format() const;
		acm::Extent2D extent() const;
		uint32_t mipLevels() const; // 1 unless created mipmapped

	private:
		// Construction
		friend acm::native::Device;
		Texture(acm::ResourceRef<acm::native::Texture> resource);
		explicit Texture(acm::Error error);

		acm::ResourceRef<acm::native::Texture> m_resource;
		acm::Error m_error;
	};
} // namespace acm
