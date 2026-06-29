#pragma once

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class Buffer;
	class DescriptorSetLayout;
	class Sampler;
	class Texture;

	class DescriptorSet : public acm::ResourceSlot<acm::vulkan::DescriptorSet, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, const acm::DescriptorSetLayout& layout);
		VkDescriptorSet vkDescriptorSet(const acm::Handle& handle) const;
		void setTexture(const acm::Handle& handle, uint32_t binding, const acm::vulkan::Texture& texture, const acm::Handle& textureHandle, const acm::vulkan::Sampler& sampler, const acm::Handle& samplerHandle, uint32_t arrayElement);
		void setBuffer(const acm::Handle& handle, uint32_t binding, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle, uint32_t arrayElement);
		void setDynamicBuffer(const acm::Handle& handle, uint32_t binding, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle, size_t elementSize, uint32_t arrayElement);
		void setStorageImage(const acm::Handle& handle, uint32_t binding, const acm::vulkan::Texture& texture, const acm::Handle& textureHandle, uint32_t arrayElement);
		void retire(acm::vulkan::Device& owner);

	private:
		VkDescriptorType bufferType(uint32_t binding) const;

		acm::DescriptorSetLayout m_layout;
		VkDescriptorPool m_pool{VK_NULL_HANDLE};
		VkDescriptorSet m_set{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
