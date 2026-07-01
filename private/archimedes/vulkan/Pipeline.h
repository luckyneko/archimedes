#pragma once

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class DescriptorSetLayout;

	class Pipeline
	{
	public:
		Pipeline() = default;
		Pipeline(acm::vulkan::Device& owner, const acm::PipelineConfig& config);
		~Pipeline();
		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;
		Pipeline(Pipeline&& other) noexcept;
		Pipeline& operator=(Pipeline&& other) noexcept;

		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_pipeline != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkPipeline vkPipeline() const;
		VkPipelineLayout vkLayout() const;

	private:
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		acm::DescriptorSetLayout m_descriptorLayout;
		VkPipelineLayout m_layout{VK_NULL_HANDLE};
		VkPipeline m_pipeline{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
