#pragma once

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;

	class Sampler
	{
	public:
		bool create(acm::vulkan::Device& owner, float maxAnisotropy);
		acm::vulkan::Device& owner() const { return *m_owner; }
		VkSampler vkSampler() const;
		void retire(acm::vulkan::Device& owner);

	private:
		acm::vulkan::Device* m_owner{nullptr};
		VkSampler m_sampler{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
