#pragma once

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmError.h"

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
		DescriptorSet() = default;
		DescriptorSet(acm::vulkan::Device& owner, const acm::DescriptorSetLayout& layout);
		~DescriptorSet();
		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;
		DescriptorSet(DescriptorSet&& other) noexcept;
		DescriptorSet& operator=(DescriptorSet&& other) noexcept;

		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_pool != VK_NULL_HANDLE && m_set != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkDescriptorSet vkDescriptorSet() const;
		void setTexture(uint32_t binding, const acm::vulkan::Texture& texture, const acm::vulkan::Sampler& sampler, uint32_t arrayElement);
		void setBuffer(uint32_t binding, const acm::vulkan::Buffer& buffer, uint32_t arrayElement);
		void setDynamicBuffer(uint32_t binding, const acm::vulkan::Buffer& buffer, size_t elementSize, uint32_t arrayElement);
		void setStorageImage(uint32_t binding, const acm::vulkan::Texture& texture, uint32_t arrayElement);

	private:
		void release();
		VkDescriptorType bufferType(uint32_t binding) const;

		acm::vulkan::Device* m_owner{nullptr};
		acm::DescriptorSetLayout m_layout;
		VkDescriptorPool m_pool{VK_NULL_HANDLE};
		VkDescriptorSet m_set{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
