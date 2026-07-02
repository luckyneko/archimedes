/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmRenderer.h"

#include "archimedes/acmCommandBuffer.h"
#include "archimedes/nativeAPI.h"

#include <utility>

acm::Renderer::Renderer() = default;

acm::Renderer::Renderer(acm::ResourceRef<acm::native::Renderer> resource)
	: m_resource(std::move(resource))
{
}

acm::Renderer::Renderer(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Renderer::Renderer(const acm::Renderer& other) = default;

acm::Renderer& acm::Renderer::operator=(const acm::Renderer& other) = default;

acm::Renderer::Renderer(acm::Renderer&& other) noexcept = default;

acm::Renderer& acm::Renderer::operator=(acm::Renderer&& other) noexcept = default;

acm::Renderer::~Renderer() = default;

void acm::Renderer::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::Renderer::valid() const
{
	return m_resource.valid();
}

acm::Error acm::Renderer::error() const
{
	return m_error;
}

acm::native::Renderer* acm::Renderer::native() const
{
	return m_resource.access();
}

acm::Error acm::Renderer::render(const std::function<void(acm::CommandBuffer&, uint32_t)>& record)
{
	if (auto* resource = m_resource.access())
		return resource->render({}, record);
	return acm::Error("invalid renderer");
}

acm::Error acm::Renderer::render(const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record)
{
	if (auto* resource = m_resource.access())
		return resource->render(prePass, record);
	return acm::Error("invalid renderer");
}
