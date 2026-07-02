/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmTypes.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Device;

	class DescriptorSetLayout
	{
	public:
		DescriptorSetLayout() = default;
		DescriptorSetLayout(acm::vulkan::Device& owner, const std::vector<acm::DescriptorBinding>& bindings);
		~DescriptorSetLayout();
		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout(DescriptorSetLayout&& other) noexcept;
		DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept;

		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_layout != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		const std::vector<acm::DescriptorBinding>* bindings() const;
		VkDescriptorSetLayout vkLayout() const;

	private:
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		std::vector<acm::DescriptorBinding> m_bindings;
		VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
