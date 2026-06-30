#pragma once

#include "archimedes/acmTypes.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Device;

	class DescriptorSetLayout
	{
	public:
		bool create(acm::vulkan::Device& owner, const std::vector<acm::DescriptorBinding>& bindings);
		acm::vulkan::Device& owner() const { return *m_owner; }
		const std::vector<acm::DescriptorBinding>* bindings() const;
		VkDescriptorSetLayout vkLayout() const;
		void retire(acm::vulkan::Device& owner);

	private:
		acm::vulkan::Device* m_owner{nullptr};
		std::vector<acm::DescriptorBinding> m_bindings;
		VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
