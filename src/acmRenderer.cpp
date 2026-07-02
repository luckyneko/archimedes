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

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Renderer::Renderer() = default;

	Renderer::Renderer(const Renderer& other) = default;

	Renderer& Renderer::operator=(const Renderer& other) = default;

	Renderer::Renderer(Renderer&& other) noexcept = default;

	Renderer& Renderer::operator=(Renderer&& other) noexcept = default;

	Renderer::~Renderer() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Renderer::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool Renderer::valid() const
	{
		return m_resource.valid();
	}

	Error Renderer::error() const
	{
		return m_error;
	}

	native::Renderer* Renderer::native() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Frames
	// -----------------------------------------------------------------------------

	Error Renderer::render(const std::function<void(CommandBuffer&, uint32_t)>& record)
	{
		if (auto* resource = m_resource.access())
			return resource->render({}, record);
		return Error("invalid renderer");
	}

	Error Renderer::render(const std::function<void(CommandBuffer&, uint32_t)>& prePass, const std::function<void(CommandBuffer&, uint32_t)>& record)
	{
		if (auto* resource = m_resource.access())
			return resource->render(prePass, record);
		return Error("invalid renderer");
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	Renderer::Renderer(ResourceRef<native::Renderer> resource)
		: m_resource(std::move(resource))
	{
	}

	Renderer::Renderer(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
