#include "archimedes/acmDescriptorSetLayout.h"
#include "acmVkConvert.h"
#include "archimedes/acmDevice.h"
#include <cassert>
#include <vector>
#include <vulkan/vulkan.h>

struct acm::DescriptorSetLayout::impl
{
	acm::Device device;
	std::vector<acm::DescriptorBinding> bindings;
	VkDescriptorSetLayout layout{VK_NULL_HANDLE};

	~impl()
	{
		if (layout && device.valid())
		{
			VkDevice dev = device.vkDevice();
			VkDescriptorSetLayout l = layout;
			device.enqueueDestroy([dev, l]
								  { vkDestroyDescriptorSetLayout(dev, l, nullptr); });
		}
	}
};

acm::DescriptorSetLayout::DescriptorSetLayout(acm::Device device, const std::vector<acm::DescriptorBinding>& bindings)
	: m()
{
	assert(!bindings.empty() && "acm::DescriptorSetLayout: no bindings");

	auto impl = std::make_shared<acm::DescriptorSetLayout::impl>();
	impl->device = device;
	impl->bindings = bindings;

	std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());
	for (size_t i = 0; i < bindings.size(); ++i)
	{
		vkBindings[i].binding = bindings[i].binding;
		vkBindings[i].descriptorType = acm::detail::toVk(bindings[i].type);
		vkBindings[i].descriptorCount = bindings[i].count; // > 1 = descriptor array
		vkBindings[i].stageFlags = acm::detail::toVk(bindings[i].stage);
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = uint32_t(vkBindings.size());
	layoutInfo.pBindings = vkBindings.data();

	if (vkCreateDescriptorSetLayout(impl->device.vkDevice(), &layoutInfo, nullptr, &impl->layout) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create descriptor set layout");
		return;
	}

	m = impl;
}

const std::vector<acm::DescriptorBinding>& acm::DescriptorSetLayout::bindings() const
{
	return m->bindings;
}

VkDescriptorSetLayout acm::DescriptorSetLayout::vkDescriptorSetLayout() const
{
	return m->layout;
}
