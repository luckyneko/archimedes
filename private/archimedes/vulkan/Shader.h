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

#include <vector>

namespace acm::vulkan
{
	class Device;

	// Move-only VkShaderModule owner built from caller-supplied SPIR-V bytecode.
	class Shader
	{
	public:
		// Lifetime
		Shader() = default;
		Shader(acm::vulkan::Device& owner, const std::vector<char>& spirv);
		~Shader();
		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;
		Shader(Shader&& other) noexcept;
		Shader& operator=(Shader&& other) noexcept;

		// State
		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_shaderModule != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkShaderModule vkShaderModule() const;

	private:
		// Internals
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		VkShaderModule m_shaderModule{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
