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
#include "archimedes/acmShader.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class DescriptorSetLayout;
	class Shader;

	// Move-only compute pipeline and pipeline layout. Retains the public descriptor
	// layout wrapper so the Vulkan layout inputs outlive the pipeline.
	class ComputePipeline
	{
	public:
		// Lifetime
		ComputePipeline() = default;
		ComputePipeline(acm::vulkan::Device& owner, const acm::Shader& compute, const acm::DescriptorSetLayout& layout);
		~ComputePipeline();
		ComputePipeline(const ComputePipeline&) = delete;
		ComputePipeline& operator=(const ComputePipeline&) = delete;
		ComputePipeline(ComputePipeline&& other) noexcept;
		ComputePipeline& operator=(ComputePipeline&& other) noexcept;

		// State
		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_pipeline != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkPipeline vkPipeline() const;
		VkPipelineLayout vkLayout() const;

	private:
		// Internals
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		acm::DescriptorSetLayout m_descriptorLayout;
		VkPipelineLayout m_layout{VK_NULL_HANDLE};
		VkPipeline m_pipeline{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
