/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class DescriptorSetLayout;
	class RenderTarget;

	// Move-only graphics pipeline and layout. Retains shaders/target/descriptor layout
	// through public wrappers supplied by PipelineConfig.
	class Pipeline
	{
	public:
		// Lifetime
		Pipeline() = default;
		Pipeline(acm::vulkan::Device& owner, const acm::PipelineConfig& config);
		~Pipeline();
		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;
		Pipeline(Pipeline&& other) noexcept;
		Pipeline& operator=(Pipeline&& other) noexcept;

		// State
		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_pipeline != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkPipeline vkPipeline() const;
		VkPipelineLayout vkLayout() const;
		VkFormat colorFormat() const { return m_colorFormat; }
		VkFormat depthFormat() const { return m_depthFormat; }
		VkSampleCountFlagBits sampleCount() const { return m_sampleCount; }
		bool compatibleWith(const acm::vulkan::RenderTarget& target) const;

	private:
		// Internals
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		acm::DescriptorSetLayout m_descriptorLayout;
		VkFormat m_colorFormat{VK_FORMAT_UNDEFINED};
		VkFormat m_depthFormat{VK_FORMAT_UNDEFINED};
		VkSampleCountFlagBits m_sampleCount{VK_SAMPLE_COUNT_1_BIT};
		VkPipelineLayout m_layout{VK_NULL_HANDLE};
		VkPipeline m_pipeline{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
