#pragma once

#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;

	class Sampler : public acm::ResourceSlot<acm::vulkan::Sampler, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, float maxAnisotropy);
		VkSampler vkSampler(const acm::Handle& handle) const;
		void retire(acm::vulkan::Device& owner);

	private:
		VkSampler m_sampler{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
