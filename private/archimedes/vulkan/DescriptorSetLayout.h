#pragma once

#include "archimedes/acmTypes.h"
#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Device;

	class DescriptorSetLayout : public acm::ResourceSlot<acm::vulkan::DescriptorSetLayout, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, const std::vector<acm::DescriptorBinding>& bindings);
		const std::vector<acm::DescriptorBinding>* bindings(const acm::Handle& handle) const;
		VkDescriptorSetLayout vkLayout(const acm::Handle& handle) const;
		void retire(acm::vulkan::Device& owner);

	private:
		std::vector<acm::DescriptorBinding> m_bindings;
		VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
