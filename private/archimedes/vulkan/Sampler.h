/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;

	// Move-only VkSampler owner. Feature fallback and clamping happen at construction.
	class Sampler
	{
	public:
		// Lifetime
		Sampler() = default;
		Sampler(acm::vulkan::Device& owner, float maxAnisotropy);
		~Sampler();
		Sampler(const Sampler&) = delete;
		Sampler& operator=(const Sampler&) = delete;
		Sampler(Sampler&& other) noexcept;
		Sampler& operator=(Sampler&& other) noexcept;

		// State
		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_sampler != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkSampler vkSampler() const;

	private:
		// Internals
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		VkSampler m_sampler{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
