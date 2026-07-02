/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmBuffer.h"

#include "archimedes/backendAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Buffer::Buffer() = default;

	Buffer::Buffer(const Buffer& other) = default;

	Buffer& Buffer::operator=(const Buffer& other) = default;

	Buffer::Buffer(Buffer&& other) noexcept = default;

	Buffer& Buffer::operator=(Buffer&& other) noexcept = default;

	Buffer::~Buffer() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Buffer::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool Buffer::valid() const
	{
		return m_resource.valid();
	}

	Error Buffer::error() const
	{
		return m_error;
	}

	backend::Buffer* Buffer::backend() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Memory
	// -----------------------------------------------------------------------------

	size_t Buffer::size() const
	{
		if (auto* resource = m_resource.access())
			return resource->size();
		return 0;
	}

	void* Buffer::map()
	{
		if (auto* resource = m_resource.access())
			return resource->map();
		return nullptr;
	}

	void Buffer::unmap()
	{
	}

	Error Buffer::write(const void* data, size_t size)
	{
		if (!m_resource)
			return Error("Buffer::write: invalid buffer");
		if (auto* resource = m_resource.access())
			return resource->write(data, size);
		return Error("Buffer::write: invalid buffer");
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	Buffer::Buffer(ResourceRef<backend::Buffer> resource)
		: m_resource(std::move(resource))
	{
	}

	Buffer::Buffer(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
