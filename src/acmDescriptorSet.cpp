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

	Error DescriptorSet::setTexture(uint32_t binding, const Texture& texture, const Sampler& sampler, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
			return resource->setTexture(binding, texture, sampler, arrayElement);
		return Error("setTexture: invalid descriptor set");
	}

	Error DescriptorSet::setBuffer(uint32_t binding, const Buffer& buffer, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
			return resource->setBuffer(binding, buffer, arrayElement);
		return Error("setBuffer: invalid descriptor set");
	}

	Error DescriptorSet::setDynamicBuffer(uint32_t binding, const Buffer& buffer, size_t elementSize, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
			return resource->setDynamicBuffer(binding, buffer, elementSize, arrayElement);
		return Error("setDynamicBuffer: invalid descriptor set");
	}

	Error DescriptorSet::setStorageImage(uint32_t binding, const Texture& texture, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
			return resource->setStorageImage(binding, texture, arrayElement);
		return Error("setStorageImage: invalid descriptor set");
	}

	Error DescriptorSet::setSampledImage(uint32_t binding, const Texture& texture, uint32_t arrayElement)
	{
		if (auto* resource = m_resource.access())
			return resource->setSampledImage(binding, texture, arrayElement);
		return Error("setSampledImage: invalid descriptor set");
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
