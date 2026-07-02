/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmDescriptorSetLayout.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::DescriptorSetLayout::DescriptorSetLayout() = default;

acm::DescriptorSetLayout::DescriptorSetLayout(acm::ResourceRef<acm::native::DescriptorSetLayout> resource)
	: m_resource(std::move(resource))
{
}

acm::DescriptorSetLayout::DescriptorSetLayout(acm::Error error)
	: m_error(std::move(error))
{
}

acm::DescriptorSetLayout::DescriptorSetLayout(const acm::DescriptorSetLayout& other) = default;

acm::DescriptorSetLayout& acm::DescriptorSetLayout::operator=(const acm::DescriptorSetLayout& other) = default;

acm::DescriptorSetLayout::DescriptorSetLayout(acm::DescriptorSetLayout&& other) noexcept = default;

acm::DescriptorSetLayout& acm::DescriptorSetLayout::operator=(acm::DescriptorSetLayout&& other) noexcept = default;

acm::DescriptorSetLayout::~DescriptorSetLayout() = default;

void acm::DescriptorSetLayout::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::DescriptorSetLayout::valid() const
{
	return m_resource.valid();
}

acm::Error acm::DescriptorSetLayout::error() const
{
	return m_error;
}

acm::native::DescriptorSetLayout* acm::DescriptorSetLayout::native() const
{
	return m_resource.access();
}
