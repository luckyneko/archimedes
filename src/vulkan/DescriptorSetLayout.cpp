/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/DescriptorSetLayout.h"

#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"

#include <utility>

acm::vulkan::DescriptorSetLayout::DescriptorSetLayout(acm::vulkan::Device& owner, const std::vector<acm::DescriptorBinding>& bindings)
{
	if (bindings.empty())
	{
		m_error = acm::Error("failed to create descriptor set layout with no bindings");
		return;
	}
	m_owner = &owner;
	std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());
	for (size_t index = 0; index < bindings.size(); ++index)
	{
		vkBindings[index].binding = bindings[index].binding;
		vkBindings[index].descriptorType = acm::vulkan::toVk(bindings[index].type);
		vkBindings[index].descriptorCount = bindings[index].count;
		vkBindings[index].stageFlags = acm::vulkan::toVk(bindings[index].stage);
	}
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = uint32_t(vkBindings.size());
	layoutInfo.pBindings = vkBindings.data();
	if (vkCreateDescriptorSetLayout(owner.vkDevice(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create descriptor set layout");
		return;
	}
	m_bindings = bindings;
}

acm::vulkan::DescriptorSetLayout::~DescriptorSetLayout()
{
	release();
}

acm::vulkan::DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
{
	*this = std::move(other);
}

acm::vulkan::DescriptorSetLayout& acm::vulkan::DescriptorSetLayout::operator=(DescriptorSetLayout&& other) noexcept
{
	if (this == &other)
		return *this;
	release();
	m_owner = std::exchange(other.m_owner, nullptr);
	m_bindings = std::move(other.m_bindings);
	m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
	m_error = std::move(other.m_error);
	return *this;
}

const std::vector<acm::DescriptorBinding>* acm::vulkan::DescriptorSetLayout::bindings() const
{
	return &m_bindings;
}

VkDescriptorSetLayout acm::vulkan::DescriptorSetLayout::vkLayout() const
{
	return m_layout;
}

void acm::vulkan::DescriptorSetLayout::release()
{
	acm::vulkan::Device* owner = std::exchange(m_owner, nullptr);
	m_bindings.clear();
	const VkDescriptorSetLayout layout = std::exchange(m_layout, VK_NULL_HANDLE);
	if (!owner || !layout)
		return;
	vkDestroyDescriptorSetLayout(owner->vkDevice(), layout, nullptr);
}
