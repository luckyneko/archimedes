#pragma once

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmForward.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class DescriptorSetLayout;

	class Pipeline
	{
	public:
		bool create(acm::vulkan::Device& owner, const acm::PipelineConfig& config);
		acm::vulkan::Device& owner() const { return *m_owner; }
		VkPipeline vkPipeline() const;
		VkPipelineLayout vkLayout() const;
		void retire(acm::vulkan::Device& owner);

	private:
		acm::vulkan::Device* m_owner{nullptr};
		acm::DescriptorSetLayout m_descriptorLayout;
		VkPipelineLayout m_layout{VK_NULL_HANDLE};
		VkPipeline m_pipeline{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
