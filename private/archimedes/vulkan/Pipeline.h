#pragma once

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmForward.h"
#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class DescriptorSetLayout;

	class Pipeline : public acm::ResourceSlot<acm::vulkan::Pipeline, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, const acm::PipelineConfig& config);
		VkPipeline vkPipeline(const acm::Handle& handle) const;
		VkPipelineLayout vkLayout(const acm::Handle& handle) const;
		void retire(acm::vulkan::Device& owner);

	private:
		acm::DescriptorSetLayout m_descriptorLayout;
		VkPipelineLayout m_layout{VK_NULL_HANDLE};
		VkPipeline m_pipeline{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
