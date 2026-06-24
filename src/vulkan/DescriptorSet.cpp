#include "archimedes/vulkan/DescriptorSet.h"

#include "archimedes/vulkan/Buffer.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/DescriptorSetLayout.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Sampler.h"
#include "archimedes/vulkan/Texture.h"

#include <cassert>
#include <map>

bool acm::vulkan::DescriptorSet::create(acm::vulkan::Device& owner, acm::vulkan::DescriptorSetLayout& layout, const acm::Handle& layoutHandle)
{
	if (&layout.owner() != &owner)
		return false;
	const auto* bindings = layout.bindings(layoutHandle);
	const VkDescriptorSetLayout vkLayout = layout.vkLayout(layoutHandle);
	if (!bindings || !vkLayout)
		return false;

	std::map<VkDescriptorType, uint32_t> counts;
	for (const acm::DescriptorBinding& binding : *bindings)
		counts[acm::vulkan::toVk(binding.type)] += binding.count;
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
		return false;

	VkDescriptorSetAllocateInfo allocationInfo = {};
	allocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocationInfo.descriptorPool = m_pool;
	allocationInfo.descriptorSetCount = 1;
	allocationInfo.pSetLayouts = &vkLayout;
	if (vkAllocateDescriptorSets(owner.vkDevice(), &allocationInfo, &m_set) != VK_SUCCESS)
		return false;
	if (!layout.retain(layoutHandle))
		return false;
	m_layoutResource = &layout;
	m_layout = layoutHandle;
	return true;
}

VkDescriptorSet acm::vulkan::DescriptorSet::vkDescriptorSet(const acm::Handle& handle) const
{
	return accessible(handle) ? m_set : VK_NULL_HANDLE;
}

void acm::vulkan::DescriptorSet::setTexture(const acm::Handle& handle, uint32_t binding, const acm::vulkan::Texture& texture, const acm::Handle& textureHandle, const acm::vulkan::Sampler& sampler, const acm::Handle& samplerHandle, uint32_t arrayElement)
{
	if (!accessible(handle) || &owner() != &texture.owner() || &owner() != &sampler.owner())
		return;
	const VkImageView imageView = texture.vkImageView(textureHandle);
	const VkSampler vkSampler = sampler.vkSampler(samplerHandle);
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

void acm::vulkan::DescriptorSet::setBuffer(const acm::Handle& handle, uint32_t binding, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle, uint32_t arrayElement)
{
	if (!accessible(handle) || &owner() != &buffer.owner())
		return;
	const VkBuffer vkBuffer = buffer.vkBuffer(bufferHandle);
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

void acm::vulkan::DescriptorSet::setDynamicBuffer(const acm::Handle& handle, uint32_t binding, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle, size_t elementSize, uint32_t arrayElement)
{
	if (!accessible(handle) || &owner() != &buffer.owner())
		return;
	const VkBuffer vkBuffer = buffer.vkBuffer(bufferHandle);
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

void acm::vulkan::DescriptorSet::setStorageImage(const acm::Handle& handle, uint32_t binding, const acm::vulkan::Texture& texture, const acm::Handle& textureHandle, uint32_t arrayElement)
{
	if (!accessible(handle) || &owner() != &texture.owner())
		return;
	const VkImageView imageView = texture.vkImageView(textureHandle);
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

VkDescriptorType acm::vulkan::DescriptorSet::bufferType(uint32_t binding) const
{
	const auto* bindings = m_layoutResource ? m_layoutResource->bindings(m_layout) : nullptr;
	if (bindings)
		for (const acm::DescriptorBinding& candidate : *bindings)
			if (candidate.binding == binding)
				return acm::vulkan::toVk(candidate.type);
	assert(false && "acm::DescriptorSet::setBuffer: unknown binding");
	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

void acm::vulkan::DescriptorSet::retire(acm::vulkan::Device& owner)
{
	acm::vulkan::DescriptorSetLayout* layoutResource = std::exchange(m_layoutResource, nullptr);
	const acm::Handle layout = std::exchange(m_layout, {});
	const VkDescriptorPool pool = std::exchange(m_pool, VK_NULL_HANDLE);
	m_set = VK_NULL_HANDLE;
	if (pool)
	{
		const VkDevice device = owner.vkDevice();
		const VkDescriptorPool retiredPool = pool;
		owner.enqueueDestroy([device, retiredPool]
							 { vkDestroyDescriptorPool(device, retiredPool, nullptr); });
	}
	if (layoutResource)
		layoutResource->release(layout);
}
