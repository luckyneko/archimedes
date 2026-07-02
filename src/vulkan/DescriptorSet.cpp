/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/DescriptorSet.h"

#include "archimedes/vulkan/Buffer.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/DescriptorSetLayout.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Sampler.h"
#include "archimedes/vulkan/Texture.h"

#include <cassert>
#include <map>
#include <utility>

namespace acm::vulkan
{

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	DescriptorSet::DescriptorSet(Device& owner, const acm::DescriptorSetLayout& layout)
	{
		if (!layout.valid() || !layout.backend() || &layout.backend()->owner() != &owner)
		{
			m_error = acm::Error("failed to create descriptor set from invalid layout");
			return;
		}
		m_owner = &owner;
		const auto* bindings = layout.backend()->bindings();
		const VkDescriptorSetLayout vkLayout = layout.backend()->vkLayout();
		if (!bindings || !vkLayout)
		{
			m_error = acm::Error("failed to create descriptor set from invalid layout");
			return;
		}

		std::map<VkDescriptorType, uint32_t> counts;
		for (const acm::DescriptorBinding& binding : *bindings)
			counts[toVk(binding.type)] += binding.count;
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.reserve(counts.size());
		for (const auto& [type, count] : counts)
			poolSizes.push_back({type, count});

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = uint32_t(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;
		if (vkCreateDescriptorPool(owner.vkDevice(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS)
		{
			m_error = acm::Error("failed to create descriptor pool");
			return;
		}

		VkDescriptorSetAllocateInfo allocationInfo = {};
		allocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocationInfo.descriptorPool = m_pool;
		allocationInfo.descriptorSetCount = 1;
		allocationInfo.pSetLayouts = &vkLayout;
		if (vkAllocateDescriptorSets(owner.vkDevice(), &allocationInfo, &m_set) != VK_SUCCESS)
		{
			m_error = acm::Error("failed to allocate descriptor set");
			return;
		}
		m_layout = layout;
	}

	DescriptorSet::~DescriptorSet()
	{
		release();
	}

	DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
	{
		*this = std::move(other);
	}

	DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other) noexcept
	{
		if (this == &other)
			return *this;
		release();
		m_owner = std::exchange(other.m_owner, nullptr);
		m_layout = std::move(other.m_layout);
		m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
		m_set = std::exchange(other.m_set, VK_NULL_HANDLE);
		m_error = std::move(other.m_error);
		return *this;
	}

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	VkDescriptorSet DescriptorSet::vkDescriptorSet() const
	{
		return m_set;
	}

	// -----------------------------------------------------------------------------
	// Writes
	// -----------------------------------------------------------------------------

	void DescriptorSet::setTexture(uint32_t binding, const Texture& texture, const Sampler& sampler, uint32_t arrayElement)
	{
		if (&owner() != &texture.owner() || &owner() != &sampler.owner())
			return;
		const VkImageView imageView = texture.vkImageView();
		const VkSampler vkSampler = sampler.vkSampler();
		if (!imageView || !vkSampler)
			return;
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = imageView;
		imageInfo.sampler = vkSampler;
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_set;
		write.dstBinding = binding;
		write.dstArrayElement = arrayElement;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.pImageInfo = &imageInfo;
		vkUpdateDescriptorSets(owner().vkDevice(), 1, &write, 0, nullptr);
	}

	void DescriptorSet::setBuffer(uint32_t binding, const Buffer& buffer, uint32_t arrayElement)
	{
		if (&owner() != &buffer.owner())
			return;
		const VkBuffer vkBuffer = buffer.vkBuffer();
		if (!vkBuffer)
			return;
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = vkBuffer;
		bufferInfo.range = VK_WHOLE_SIZE;
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_set;
		write.dstBinding = binding;
		write.dstArrayElement = arrayElement;
		write.descriptorCount = 1;
		write.descriptorType = bufferType(binding);
		write.pBufferInfo = &bufferInfo;
		vkUpdateDescriptorSets(owner().vkDevice(), 1, &write, 0, nullptr);
	}

	void DescriptorSet::setDynamicBuffer(uint32_t binding, const Buffer& buffer, size_t elementSize, uint32_t arrayElement)
	{
		if (&owner() != &buffer.owner())
			return;
		const VkBuffer vkBuffer = buffer.vkBuffer();
		if (!vkBuffer)
			return;
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = vkBuffer;
		bufferInfo.range = elementSize;
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_set;
		write.dstBinding = binding;
		write.dstArrayElement = arrayElement;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write.pBufferInfo = &bufferInfo;
		vkUpdateDescriptorSets(owner().vkDevice(), 1, &write, 0, nullptr);
	}

	void DescriptorSet::setStorageImage(uint32_t binding, const Texture& texture, uint32_t arrayElement)
	{
		if (&owner() != &texture.owner())
			return;
		const VkImageView imageView = texture.vkImageView();
		if (!imageView)
			return;
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfo.imageView = imageView;
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_set;
		write.dstBinding = binding;
		write.dstArrayElement = arrayElement;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		write.pImageInfo = &imageInfo;
		vkUpdateDescriptorSets(owner().vkDevice(), 1, &write, 0, nullptr);
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	void DescriptorSet::release()
	{
		Device* owner = std::exchange(m_owner, nullptr);
		const VkDescriptorPool pool = std::exchange(m_pool, VK_NULL_HANDLE);
		m_set = VK_NULL_HANDLE;
		if (owner && pool)
			vkDestroyDescriptorPool(owner->vkDevice(), pool, nullptr);
		m_layout.reset();
	}

	VkDescriptorType DescriptorSet::bufferType(uint32_t binding) const
	{
		const auto* bindings = m_layout.valid() ? m_layout.backend()->bindings() : nullptr;
		if (bindings)
			for (const acm::DescriptorBinding& candidate : *bindings)
				if (candidate.binding == binding)
					return toVk(candidate.type);
		assert(false && "acm::DescriptorSet::setBuffer: unknown binding");
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	}

} // namespace acm::vulkan
