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
	class DescriptorSet;
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

		// Descriptors
		// Whether the pipeline was built with a descriptor layout (set 0) at all.
		bool hasDescriptorLayout() const { return m_descriptorLayoutIdentity != nullptr; }
		// Whether `set` can be bound as set 0: allocated from this pipeline's descriptor
		// layout, or from one with identical bindings (as Pipeline::acceptsDescriptorSet).
		bool acceptsDescriptorSet(const acm::vulkan::DescriptorSet& set) const;

	private:
		// Internals
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		acm::DescriptorSetLayout m_descriptorLayout;
		// The retained layout's backend, kept for identity comparison on every bind.
		const acm::vulkan::DescriptorSetLayout* m_descriptorLayoutIdentity{nullptr};
		VkPipelineLayout m_layout{VK_NULL_HANDLE};
		VkPipeline m_pipeline{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
