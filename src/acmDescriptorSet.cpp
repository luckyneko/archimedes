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
#include "archimedes/backendAPI.h"

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

	backend::DescriptorSet* DescriptorSet::backend() const
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
			if (texture.backend() && sampler.backend())
				resource->setTexture(binding, *texture.backend(), *sampler.backend(), arrayElement);
		}
	}

	void DescriptorSet::setBuffer(uint32_t binding, const Buffer& buffer, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
		{
			if (buffer.backend())
				resource->setBuffer(binding, *buffer.backend(), arrayElement);
		}
	}

	void DescriptorSet::setDynamicBuffer(uint32_t binding, const Buffer& buffer, size_t elementSize, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
		{
			if (buffer.backend())
				resource->setDynamicBuffer(binding, *buffer.backend(), elementSize, arrayElement);
		}
	}

	void DescriptorSet::setStorageImage(uint32_t binding, const Texture& texture, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
		{
			if (texture.backend())
				resource->setStorageImage(binding, *texture.backend(), arrayElement);
		}
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	DescriptorSet::DescriptorSet(ResourceRef<backend::DescriptorSet> resource)
		: m_resource(std::move(resource))
	{
	}

	DescriptorSet::DescriptorSet(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
