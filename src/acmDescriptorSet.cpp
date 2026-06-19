#include "archimedes/acmDescriptorSet.h"
#include "acmVkConvert.h"
#include "archimedes/acmBuffer.h"
#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmDevice.h"
#include "archimedes/acmSampler.h"
#include "archimedes/acmTexture.h"
#include <cassert>
#include <map>
#include <vector>
#include <vulkan/vulkan.h>

struct acm::DescriptorSet::impl
{
	acm::Device device;
	acm::DescriptorSetLayout layout; // kept alive while the set references it
	VkDescriptorPool pool{VK_NULL_HANDLE};
	VkDescriptorSet set{VK_NULL_HANDLE}; // freed with the pool

	~impl()
	{
		if (pool && device.valid())
		{
			VkDevice dev = device.vkDevice();
			VkDescriptorPool p = pool;
			device.enqueueDestroy([dev, p]
								  { vkDestroyDescriptorPool(dev, p, nullptr); });
		}
	}
};

acm::DescriptorSet::DescriptorSet(acm::Device device, acm::DescriptorSetLayout layout)
	: m()
{
	assert(layout.valid() && !layout.bindings().empty() && "acm::DescriptorSet: invalid/empty layout");

	auto impl = std::make_shared<acm::DescriptorSet::impl>();
	impl->device = device;
	impl->layout = layout;

	// One pool size per descriptor type, summing the descriptors each binding needs
	// (a binding's `count` > 1 is a descriptor array).
	std::map<VkDescriptorType, uint32_t> counts;
	for (const auto& b : layout.bindings())
		counts[acm::detail::toVk(b.type)] += b.count;
	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(counts.size());
	for (const auto& [type, count] : counts)
		poolSizes.push_back({type, count});

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = uint32_t(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;
	if (vkCreateDescriptorPool(impl->device.vkDevice(), &poolInfo, nullptr, &impl->pool) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create descriptor pool");
		return;
	}

	VkDescriptorSetLayout vkLayout = layout.vkDescriptorSetLayout();
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = impl->pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vkLayout;
	if (vkAllocateDescriptorSets(impl->device.vkDevice(), &allocInfo, &impl->set) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to allocate descriptor set");
		return;
	}

	m = impl;
}

namespace
{
	// The descriptor type the layout declared at `binding`.
	VkDescriptorType bufferTypeAt(const acm::DescriptorSetLayout& layout, uint32_t binding)
	{
		for (const auto& b : layout.bindings())
			if (b.binding == binding)
				return acm::detail::toVk(b.type);
		assert(false && "acm::DescriptorSet::setBuffer: unknown binding");
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	}
} // namespace

void acm::DescriptorSet::setTexture(uint32_t binding, acm::Texture texture, acm::Sampler sampler, uint32_t arrayElement)
{
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = texture.vkImageView();
	imageInfo.sampler = sampler.vkSampler();

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m->set;
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m->device.vkDevice(), 1, &write, 0, nullptr);
}

void acm::DescriptorSet::setBuffer(uint32_t binding, acm::Buffer buffer, uint32_t arrayElement)
{
	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = buffer.vkBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m->set;
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = 1;
	write.descriptorType = bufferTypeAt(m->layout, binding); // uniform or storage, per the layout
	write.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(m->device.vkDevice(), 1, &write, 0, nullptr);
}

void acm::DescriptorSet::setDynamicBuffer(uint32_t binding, acm::Buffer buffer, size_t elementSize, uint32_t arrayElement)
{
	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = buffer.vkBuffer();
	bufferInfo.offset = 0;			// base; the per-draw offset is the dynamic offset at bind
	bufferInfo.range = elementSize; // one object's worth — what a single draw reads

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m->set;
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; // layout binding must match
	write.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(m->device.vkDevice(), 1, &write, 0, nullptr);
}

void acm::DescriptorSet::setStorageImage(uint32_t binding, acm::Texture texture, uint32_t arrayElement)
{
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // storage images are accessed in GENERAL
	imageInfo.imageView = texture.vkImageView();
	imageInfo.sampler = VK_NULL_HANDLE; // no sampler for a storage image

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m->set;
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m->device.vkDevice(), 1, &write, 0, nullptr);
}

VkDescriptorSet acm::DescriptorSet::vkDescriptorSet() const
{
	return m->set;
}
