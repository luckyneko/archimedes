/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmBuffer.h"
#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmError.h"
#include "archimedes/acmSampler.h"
#include "archimedes/acmTexture.h"
#include "archimedes/acmTypes.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace acm::vulkan
{
	class Device;
	class DescriptorSetLayout;

	// Move-only one-set descriptor pool + descriptor set allocated from a retained
	// DescriptorSetLayout wrapper. Writes check the binding, array element, descriptor type
	// and resources before updating the native set, and each written (binding, array element)
	// retains the public wrappers it points at until it is rewritten or the set is destroyed,
	// so callers may drop their own wrappers while the set still references them. Mutating
	// one set is caller-synchronized.
	class DescriptorSet
	{
	public:
		// Lifetime
		DescriptorSet() = default;
		DescriptorSet(acm::vulkan::Device& owner, const acm::DescriptorSetLayout& layout);
		~DescriptorSet();
		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;
		DescriptorSet(DescriptorSet&& other) noexcept;
		DescriptorSet& operator=(DescriptorSet&& other) noexcept;

		// State
		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_pool != VK_NULL_HANDLE && m_set != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkDescriptorSet vkDescriptorSet() const;

		// Layout
		// True when `layout` (with `bindings`) is the layout this set was allocated from, or one
		// with identical bindings — the Vulkan rule for binding this set against a pipeline
		// layout built from it. `layout` is compared by identity only, never dereferenced.
		bool matchesLayout(const acm::vulkan::DescriptorSetLayout* layout, const std::vector<acm::DescriptorBinding>* bindings) const;
		// Descriptors of type UniformBufferDynamic in the layout; a bind supplies one offset each.
		uint32_t dynamicDescriptorCount() const { return m_dynamicDescriptorCount; }
		// The range and buffer size of the one written dynamic uniform; false when the layout
		// does not hold exactly one or it is not written yet.
		bool dynamicBufferExtent(size_t& range, size_t& bufferSize) const;

		// Writes
		acm::Error setTexture(uint32_t binding, const acm::Texture& texture, const acm::Sampler& sampler, uint32_t arrayElement);
		acm::Error setBuffer(uint32_t binding, const acm::Buffer& buffer, uint32_t arrayElement);
		acm::Error setDynamicBuffer(uint32_t binding, const acm::Buffer& buffer, size_t elementSize, uint32_t arrayElement);
		acm::Error setStorageImage(uint32_t binding, const acm::Texture& texture, uint32_t arrayElement);
		acm::Error setSampledImage(uint32_t binding, const acm::Texture& texture, uint32_t arrayElement);

	private:
		// What one (binding, array element) currently points at, retained.
		struct Written
		{
			acm::Texture texture;
			acm::Sampler sampler;
			acm::Buffer buffer;
			size_t range{0};
			size_t bufferSize{0};
		};

		// Internals
		void release();
		// "binding N is <actual>, not <expected>", for a write that does not fit its binding.
		static std::string typeMismatch(uint32_t binding, acm::DescriptorType actual, const char* expected);
		acm::Error locate(const char* call, uint32_t binding, uint32_t arrayElement, size_t& slot, acm::DescriptorType& type) const;
		acm::Error checkImage(const char* call, const acm::Texture& texture) const;
		acm::Error checkBuffer(const char* call, const acm::Buffer& buffer) const;
		// After checkBuffer: whether the buffer's Vulkan usage has `required` for a `type`
		// descriptor at `binding`; `usage` names the BufferUsage that would.
		static acm::Error checkBufferUsage(const char* call, const acm::Buffer& buffer, VkBufferUsageFlags required, uint32_t binding, acm::DescriptorType type, const char* usage);
		void writeImage(uint32_t binding, uint32_t arrayElement, VkDescriptorType type, VkImageView view, VkSampler sampler, VkImageLayout layout) const;
		void writeBuffer(uint32_t binding, uint32_t arrayElement, VkDescriptorType type, VkBuffer buffer, VkDeviceSize range) const;
		static acm::Error fail(const char* call, const std::string& reason);

		acm::vulkan::Device* m_owner{nullptr};
		acm::DescriptorSetLayout m_layout;
		const acm::vulkan::DescriptorSetLayout* m_layoutIdentity{nullptr};
		std::vector<acm::DescriptorBinding> m_bindings;
		std::vector<size_t> m_firstSlot; // per m_bindings entry: its element 0 in m_written
		std::vector<Written> m_written;	 // one per descriptor (binding x array element)
		uint32_t m_dynamicDescriptorCount{0};
		size_t m_dynamicSlot{0}; // m_written index of the dynamic uniform when there is one
		VkDescriptorPool m_pool{VK_NULL_HANDLE};
		VkDescriptorSet m_set{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
