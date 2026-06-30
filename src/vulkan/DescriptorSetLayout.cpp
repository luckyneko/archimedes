#include "archimedes/vulkan/DescriptorSetLayout.h"

#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"

bool acm::vulkan::DescriptorSetLayout::create(acm::vulkan::Device& owner, const std::vector<acm::DescriptorBinding>& bindings)
{
	if (bindings.empty())
		return false;
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
		return false;
	m_bindings = bindings;
	return true;
}

const std::vector<acm::DescriptorBinding>* acm::vulkan::DescriptorSetLayout::bindings() const
{
	return &m_bindings;
}

VkDescriptorSetLayout acm::vulkan::DescriptorSetLayout::vkLayout() const
{
	return m_layout;
}

void acm::vulkan::DescriptorSetLayout::retire(acm::vulkan::Device& owner)
{
	m_bindings.clear();
	const VkDescriptorSetLayout layout = std::exchange(m_layout, VK_NULL_HANDLE);
	m_owner = nullptr;
	if (!layout)
		return;
	const VkDevice device = owner.vkDevice();
	const VkDescriptorSetLayout retiredLayout = layout;
	owner.enqueueDestroy([device, retiredLayout]
						 { vkDestroyDescriptorSetLayout(device, retiredLayout, nullptr); });
}
