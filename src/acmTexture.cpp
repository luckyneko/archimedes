/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmTexture.h"

#include "archimedes/nativeAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Texture::Texture() = default;

	Texture::Texture(const Texture& other) = default;

	Texture& Texture::operator=(const Texture& other) = default;

	Texture::Texture(Texture&& other) noexcept = default;

	Texture& Texture::operator=(Texture&& other) noexcept = default;

	Texture::~Texture() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Texture::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool Texture::valid() const
	{
		return m_resource.valid();
	}

	Error Texture::error() const
	{
		return m_error;
	}

	native::Texture* Texture::native() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Upload
	// -----------------------------------------------------------------------------

	Error Texture::upload(const void* pixels, size_t size)
	{
		if (!m_resource)
			return Error("Texture::upload: invalid texture");
		if (auto* resource = m_resource.access())
			return resource->upload(pixels, size);
		return Error("Texture::upload: invalid texture");
	}

	// -----------------------------------------------------------------------------
	// Properties
	// -----------------------------------------------------------------------------

	Format Texture::format() const
	{
		if (auto* resource = m_resource.access())
			return resource->format();
		return Format::Undefined;
	}

	Extent2D Texture::extent() const
	{
		if (auto* resource = m_resource.access())
			return resource->extent();
		return Extent2D{};
	}

	uint32_t Texture::mipLevels() const
	{
		if (auto* resource = m_resource.access())
			return resource->mipLevels();
		return 0;
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	Texture::Texture(ResourceRef<native::Texture> resource)
		: m_resource(std::move(resource))
	{
	}

	Texture::Texture(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
