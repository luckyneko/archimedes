/*
 *  Created by LuckyNeko on 19/06/2026.
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

#include <cstdint>
#include <functional>

namespace acm
{
	// Copyable handle to the swapchain frame loop. render() waits the current
	// frame-in-flight fence, records into that slot's CommandBuffer, submits, and
	// presents; frameIndex identifies the safe per-frame resource slot to update.
	class Renderer
	{
	public:
		static constexpr uint32_t MaxFramesInFlight = 2;

		// Lifetime
		Renderer();
		Renderer(const acm::Renderer& other);
		Renderer& operator=(const acm::Renderer& other);
		Renderer(acm::Renderer&& other) noexcept;
		Renderer& operator=(acm::Renderer&& other) noexcept;
		~Renderer();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::backend::Renderer* backend() const;

		// Frames
		// record runs inside dynamic rendering for the current swapchain image. prePass
		// runs first, outside any rendering scope, for compute/transitions/barriers that
		// feed the draw pass.
		acm::Error render(const std::function<void(acm::CommandBuffer& cmd, uint32_t frameIndex)>& record);
		acm::Error render(const std::function<void(acm::CommandBuffer& cmd, uint32_t frameIndex)>& prePass,
						  const std::function<void(acm::CommandBuffer& cmd, uint32_t frameIndex)>& record);

	private:
		// Construction
		friend acm::backend::Device;
		Renderer(acm::ResourceRef<acm::backend::Renderer> resource);
		explicit Renderer(acm::Error error);

		acm::ResourceRef<acm::backend::Renderer> m_resource;
		acm::Error m_error;
	};
} // namespace acm
