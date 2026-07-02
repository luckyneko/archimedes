/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmDescriptorSet.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/acmSampler.h"
#include "archimedes/acmTexture.h"
#include "archimedes/nativeAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	DescriptorSet::DescriptorSet() = default;

	DescriptorSet::DescriptorSet(const DescriptorSet& other) = default;

	DescriptorSet& DescriptorSet::operator=(const DescriptorSet& other) = default;

	DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept = default;

	DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other) noexcept = default;

	DescriptorSet::~DescriptorSet() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void DescriptorSet::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool DescriptorSet::valid() const
	{
		return m_resource.valid();
	}

	Error DescriptorSet::error() const
	{
		return m_error;
	}

	native::DescriptorSet* DescriptorSet::native() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Writes
	// -----------------------------------------------------------------------------

	void DescriptorSet::setTexture(uint32_t binding, const Texture& texture, const Sampler& sampler, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
		{
			if (texture.native() && sampler.native())
				resource->setTexture(binding, *texture.native(), *sampler.native(), arrayElement);
		}
	}

	void DescriptorSet::setBuffer(uint32_t binding, const Buffer& buffer, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
		{
			if (buffer.native())
				resource->setBuffer(binding, *buffer.native(), arrayElement);
		}
	}

	void DescriptorSet::setDynamicBuffer(uint32_t binding, const Buffer& buffer, size_t elementSize, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
		{
			if (buffer.native())
				resource->setDynamicBuffer(binding, *buffer.native(), elementSize, arrayElement);
		}
	}

	void DescriptorSet::setStorageImage(uint32_t binding, const Texture& texture, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
		{
			if (texture.native())
				resource->setStorageImage(binding, *texture.native(), arrayElement);
		}
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	DescriptorSet::DescriptorSet(ResourceRef<native::DescriptorSet> resource)
		: m_resource(std::move(resource))
	{
	}

	DescriptorSet::DescriptorSet(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
