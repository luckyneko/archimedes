#pragma once

#include "archimedes/acmDescriptorSetLayout.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class Buffer;
	class DescriptorSetLayout;
	class Sampler;
	class Texture;

	class DescriptorSet
	{
	public:
		bool create(acm::vulkan::Device& owner, const acm::DescriptorSetLayout& layout);
		acm::vulkan::Device& owner() const { return *m_owner; }
		VkDescriptorSet vkDescriptorSet() const;
		void setTexture(uint32_t binding, const acm::vulkan::Texture& texture, const acm::vulkan::Sampler& sampler, uint32_t arrayElement);
		void setBuffer(uint32_t binding, const acm::vulkan::Buffer& buffer, uint32_t arrayElement);
		void setDynamicBuffer(uint32_t binding, const acm::vulkan::Buffer& buffer, size_t elementSize, uint32_t arrayElement);
		void setStorageImage(uint32_t binding, const acm::vulkan::Texture& texture, uint32_t arrayElement);
		void retire(acm::vulkan::Device& owner);

	private:
		VkDescriptorType bufferType(uint32_t binding) const;

		acm::vulkan::Device* m_owner{nullptr};
		acm::DescriptorSetLayout m_layout;
		VkDescriptorPool m_pool{VK_NULL_HANDLE};
		VkDescriptorSet m_set{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
