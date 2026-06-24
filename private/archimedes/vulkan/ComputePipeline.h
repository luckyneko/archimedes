#pragma once

#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class DescriptorSetLayout;
	class Shader;

	class ComputePipeline : public acm::ResourceSlot<acm::vulkan::ComputePipeline, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, acm::vulkan::Shader& compute, const acm::Handle& computeHandle, acm::vulkan::DescriptorSetLayout* layout, const acm::Handle& layoutHandle);
		VkPipeline vkPipeline(const acm::Handle& handle) const;
		VkPipelineLayout vkLayout(const acm::Handle& handle) const;
		void retire(acm::vulkan::Device& owner);

	private:
		acm::vulkan::DescriptorSetLayout* m_descriptorLayoutResource{nullptr};
		acm::Handle m_descriptorLayout;
		VkPipelineLayout m_layout{VK_NULL_HANDLE};
		VkPipeline m_pipeline{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
