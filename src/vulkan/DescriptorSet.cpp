/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/DescriptorSet.h"

#include "archimedes/vulkan/Buffer.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/DescriptorSetLayout.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Sampler.h"
#include "archimedes/vulkan/Texture.h"

#include <map>
#include <utility>

namespace acm::vulkan
{
	namespace
	{
		const char* typeName(acm::DescriptorType type)
		{
			switch (type)
			{
				case acm::DescriptorType::UniformBuffer:
					return "UniformBuffer";
				case acm::DescriptorType::CombinedImageSampler:
					return "CombinedImageSampler";
				case acm::DescriptorType::StorageBuffer:
					return "StorageBuffer";
				case acm::DescriptorType::UniformBufferDynamic:
					return "UniformBufferDynamic";
				case acm::DescriptorType::StorageImage:
					return "StorageImage";
				case acm::DescriptorType::SampledImage:
					return "SampledImage";
			}
			return "an unknown type";
		}

		std::string typeMismatch(uint32_t binding, acm::DescriptorType actual, const char* expected)
		{
			return "binding " + std::to_string(binding) + " is " + typeName(actual) + ", not " + expected;
		}

		bool isDepthFormat(acm::Format format)
		{
			return format == acm::Format::D32_Sfloat || format == acm::Format::D24_Unorm_S8_Uint;
		}

		bool sameBindings(const std::vector<acm::DescriptorBinding>& a, const std::vector<acm::DescriptorBinding>& b)
		{
			if (a.size() != b.size())
				return false;
			for (const acm::DescriptorBinding& left : a)
			{
				bool matched = false;
				for (const acm::DescriptorBinding& right : b)
					if (left.binding == right.binding)
					{
						matched = left.type == right.type && left.stage == right.stage && left.count == right.count;
						break;
					}
				if (!matched)
					return false;
			}
			return true;
		}
	} // namespace

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	DescriptorSet::DescriptorSet(Device& owner, const acm::DescriptorSetLayout& layout)
	{
		if (!layout.valid() || !layout.backend() || &layout.backend()->owner() != &owner)
		{
			m_error = acm::Error("failed to create descriptor set from invalid layout");
			return;
		}
		m_owner = &owner;
		const auto* bindings = layout.backend()->bindings();
		const VkDescriptorSetLayout vkLayout = layout.backend()->vkLayout();
		if (!bindings || !vkLayout)
		{
			m_error = acm::Error("failed to create descriptor set from invalid layout");
			return;
		}

		std::map<VkDescriptorType, uint32_t> counts;
		for (const acm::DescriptorBinding& binding : *bindings)
			counts[toVk(binding.type)] += binding.count;
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.reserve(counts.size());
		for (const auto& [type, count] : counts)
			poolSizes.push_back({type, count});

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = uint32_t(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;
		if (vkCreateDescriptorPool(owner.vkDevice(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS)
		{
			m_error = acm::Error("failed to create descriptor pool");
			return;
		}

		VkDescriptorSetAllocateInfo allocationInfo = {};
		allocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocationInfo.descriptorPool = m_pool;
		allocationInfo.descriptorSetCount = 1;
		allocationInfo.pSetLayouts = &vkLayout;
		if (vkAllocateDescriptorSets(owner.vkDevice(), &allocationInfo, &m_set) != VK_SUCCESS)
		{
			m_error = acm::Error("failed to allocate descriptor set");
			return;
		}

		// One retention slot per descriptor, laid out binding by binding.
		m_bindings = *bindings;
		m_firstSlot.reserve(m_bindings.size());
		size_t slots = 0;
		for (const acm::DescriptorBinding& binding : m_bindings)
		{
			m_firstSlot.push_back(slots);
			if (binding.type == acm::DescriptorType::UniformBufferDynamic)
			{
				m_dynamicDescriptorCount += binding.count;
				m_dynamicSlot = slots;
			}
			slots += binding.count;
		}
		m_written.resize(slots);
		m_layoutIdentity = layout.backend();
		m_layout = layout;
	}

	DescriptorSet::~DescriptorSet()
	{
		release();
	}

	DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
	{
		*this = std::move(other);
	}

	DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other) noexcept
	{
		if (this == &other)
			return *this;
		release();
		m_owner = std::exchange(other.m_owner, nullptr);
		m_layout = std::move(other.m_layout);
		m_layoutIdentity = std::exchange(other.m_layoutIdentity, nullptr);
		m_bindings = std::move(other.m_bindings);
		m_firstSlot = std::move(other.m_firstSlot);
		m_written = std::move(other.m_written);
		m_dynamicDescriptorCount = std::exchange(other.m_dynamicDescriptorCount, 0u);
		m_dynamicSlot = std::exchange(other.m_dynamicSlot, size_t(0));
		m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
		m_set = std::exchange(other.m_set, VK_NULL_HANDLE);
		m_error = std::move(other.m_error);
		return *this;
	}

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	VkDescriptorSet DescriptorSet::vkDescriptorSet() const
	{
		return m_set;
	}

	// -----------------------------------------------------------------------------
	// Layout
	// -----------------------------------------------------------------------------

	bool DescriptorSet::matchesLayout(const DescriptorSetLayout* layout, const std::vector<acm::DescriptorBinding>* bindings) const
	{
		if (!layout || !m_layoutIdentity)
			return false;
		if (layout == m_layoutIdentity)
			return true;
		return bindings && sameBindings(*bindings, m_bindings);
	}

	bool DescriptorSet::dynamicBufferExtent(size_t& range, size_t& bufferSize) const
	{
		if (m_dynamicDescriptorCount != 1 || m_dynamicSlot >= m_written.size())
			return false;
		const Written& written = m_written[m_dynamicSlot];
		if (!written.buffer.valid())
			return false;
		range = written.range;
		bufferSize = written.bufferSize;
		return true;
	}

	// -----------------------------------------------------------------------------
	// Writes
	// -----------------------------------------------------------------------------

	acm::Error DescriptorSet::setTexture(uint32_t binding, const acm::Texture& texture, const acm::Sampler& sampler, uint32_t arrayElement)
	{
		constexpr const char* call = "setTexture";
		size_t slot = 0;
		acm::DescriptorType type{};
		if (acm::Error error = locate(call, binding, arrayElement, slot, type))
			return error;
		if (type != acm::DescriptorType::CombinedImageSampler)
			return fail(call, typeMismatch(binding, type, "CombinedImageSampler"));
		if (acm::Error error = checkImage(call, texture))
			return error;
		const Sampler* backendSampler = sampler.backend();
		if (!backendSampler || !backendSampler->vkSampler())
			return fail(call, "invalid sampler");
		if (&backendSampler->owner() != m_owner)
			return fail(call, "sampler is owned by another device");

		writeImage(binding, arrayElement, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texture.backend()->vkImageView(), backendSampler->vkSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		m_written[slot] = Written{texture, sampler, acm::Buffer{}, 0, 0};
		return {};
	}

	acm::Error DescriptorSet::setBuffer(uint32_t binding, const acm::Buffer& buffer, uint32_t arrayElement)
	{
		constexpr const char* call = "setBuffer";
		size_t slot = 0;
		acm::DescriptorType type{};
		if (acm::Error error = locate(call, binding, arrayElement, slot, type))
			return error;
		if (type == acm::DescriptorType::UniformBufferDynamic)
			return fail(call, typeMismatch(binding, type, "UniformBuffer or StorageBuffer; write it with setDynamicBuffer"));
		if (type != acm::DescriptorType::UniformBuffer && type != acm::DescriptorType::StorageBuffer)
			return fail(call, typeMismatch(binding, type, "UniformBuffer or StorageBuffer"));
		if (acm::Error error = checkBuffer(call, buffer))
			return error;
		const bool uniform = type == acm::DescriptorType::UniformBuffer;
		if (acm::Error error = checkBufferUsage(call, buffer, uniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, binding, type, uniform ? "Uniform" : "Storage"))
			return error;
		const size_t size = buffer.backend()->size();
		const VkPhysicalDeviceLimits& limits = m_owner->properties().limits;
		if (type == acm::DescriptorType::UniformBuffer && size > limits.maxUniformBufferRange)
			return fail(call, "buffer size " + std::to_string(size) + " exceeds the device's maxUniformBufferRange " + std::to_string(limits.maxUniformBufferRange));
		if (type == acm::DescriptorType::StorageBuffer && size > limits.maxStorageBufferRange)
			return fail(call, "buffer size " + std::to_string(size) + " exceeds the device's maxStorageBufferRange " + std::to_string(limits.maxStorageBufferRange));

		writeBuffer(binding, arrayElement, toVk(type), buffer.backend()->vkBuffer(), VK_WHOLE_SIZE);
		m_written[slot] = Written{acm::Texture{}, acm::Sampler{}, buffer, size, size};
		return {};
	}

	acm::Error DescriptorSet::setDynamicBuffer(uint32_t binding, const acm::Buffer& buffer, size_t elementSize, uint32_t arrayElement)
	{
		constexpr const char* call = "setDynamicBuffer";
		size_t slot = 0;
		acm::DescriptorType type{};
		if (acm::Error error = locate(call, binding, arrayElement, slot, type))
			return error;
		if (type != acm::DescriptorType::UniformBufferDynamic)
			return fail(call, typeMismatch(binding, type, "UniformBufferDynamic"));
		if (acm::Error error = checkBuffer(call, buffer))
			return error;
		if (acm::Error error = checkBufferUsage(call, buffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, binding, type, "Uniform"))
			return error;
		const size_t size = buffer.backend()->size();
		const uint32_t maxRange = m_owner->properties().limits.maxUniformBufferRange;
		if (elementSize == 0)
			return fail(call, "element size must be non-zero");
		if (elementSize > size)
			return fail(call, "element size " + std::to_string(elementSize) + " exceeds the buffer size " + std::to_string(size));
		if (elementSize > maxRange)
			return fail(call, "element size " + std::to_string(elementSize) + " exceeds the device's maxUniformBufferRange " + std::to_string(maxRange));

		writeBuffer(binding, arrayElement, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, buffer.backend()->vkBuffer(), VkDeviceSize(elementSize));
		m_written[slot] = Written{acm::Texture{}, acm::Sampler{}, buffer, elementSize, size};
		return {};
	}

	acm::Error DescriptorSet::setStorageImage(uint32_t binding, const acm::Texture& texture, uint32_t arrayElement)
	{
		constexpr const char* call = "setStorageImage";
		size_t slot = 0;
		acm::DescriptorType type{};
		if (acm::Error error = locate(call, binding, arrayElement, slot, type))
			return error;
		if (type != acm::DescriptorType::StorageImage)
			return fail(call, typeMismatch(binding, type, "StorageImage"));
		if (acm::Error error = checkImage(call, texture))
			return error;
		if (!texture.backend()->storage())
			return fail(call, "binding " + std::to_string(binding) + " is StorageImage, but the texture was not created with TextureConfig::storage");

		writeImage(binding, arrayElement, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, texture.backend()->vkImageView(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
		m_written[slot] = Written{texture, acm::Sampler{}, acm::Buffer{}, 0, 0};
		return {};
	}

	acm::Error DescriptorSet::setSampledImage(uint32_t binding, const acm::Texture& texture, uint32_t arrayElement)
	{
		constexpr const char* call = "setSampledImage";
		size_t slot = 0;
		acm::DescriptorType type{};
		if (acm::Error error = locate(call, binding, arrayElement, slot, type))
			return error;
		if (type != acm::DescriptorType::SampledImage)
			return fail(call, typeMismatch(binding, type, "SampledImage"));
		if (acm::Error error = checkImage(call, texture))
			return error;

		writeImage(binding, arrayElement, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, texture.backend()->vkImageView(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		m_written[slot] = Written{texture, acm::Sampler{}, acm::Buffer{}, 0, 0};
		return {};
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	void DescriptorSet::release()
	{
		Device* owner = std::exchange(m_owner, nullptr);
		const VkDescriptorPool pool = std::exchange(m_pool, VK_NULL_HANDLE);
		m_set = VK_NULL_HANDLE;
		if (owner && pool)
			vkDestroyDescriptorPool(owner->vkDevice(), pool, nullptr);
		// The native set is gone, so what it pointed at may go too.
		m_written.clear();
		m_firstSlot.clear();
		m_bindings.clear();
		m_dynamicDescriptorCount = 0;
		m_dynamicSlot = 0;
		m_layoutIdentity = nullptr;
		m_layout.reset();
	}

	acm::Error DescriptorSet::locate(const char* call, uint32_t binding, uint32_t arrayElement, size_t& slot, acm::DescriptorType& type) const
	{
		for (size_t i = 0; i < m_bindings.size(); ++i)
		{
			const acm::DescriptorBinding& candidate = m_bindings[i];
			if (candidate.binding != binding)
				continue;
			if (arrayElement >= candidate.count)
				return fail(call, "array element " + std::to_string(arrayElement) + " is out of range for binding " + std::to_string(binding) + " (count " + std::to_string(candidate.count) + ")");
			slot = m_firstSlot[i] + arrayElement;
			type = candidate.type;
			return {};
		}
		return fail(call, "binding " + std::to_string(binding) + " is not in the descriptor set layout");
	}

	acm::Error DescriptorSet::checkImage(const char* call, const acm::Texture& texture) const
	{
		const Texture* backendTexture = texture.backend();
		if (!backendTexture || !backendTexture->vkImageView())
			return fail(call, "invalid texture");
		if (&backendTexture->owner() != m_owner)
			return fail(call, "texture is owned by another device");
		if (isDepthFormat(backendTexture->format()))
			return fail(call, "a depth texture cannot be bound to a shader descriptor");
		return {};
	}

	acm::Error DescriptorSet::checkBuffer(const char* call, const acm::Buffer& buffer) const
	{
		const Buffer* backendBuffer = buffer.backend();
		if (!backendBuffer || !backendBuffer->vkBuffer())
			return fail(call, "invalid buffer");
		if (&backendBuffer->owner() != m_owner)
			return fail(call, "buffer is owned by another device");
		return {};
	}

	acm::Error DescriptorSet::checkBufferUsage(const char* call, const acm::Buffer& buffer, VkBufferUsageFlags required, uint32_t binding, acm::DescriptorType type, const char* usage)
	{
		const Buffer* backendBuffer = buffer.backend();
		if (backendBuffer->vkUsage() & required)
			return {};
		return fail(call, "binding " + std::to_string(binding) + " is " + typeName(type) + ", which a BufferUsage::" + bufferUsageName(backendBuffer->usage()) + " buffer cannot back; create it with BufferUsage::" + usage);
	}

	void DescriptorSet::writeImage(uint32_t binding, uint32_t arrayElement, VkDescriptorType type, VkImageView view, VkSampler sampler, VkImageLayout layout) const
	{
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = layout;
		imageInfo.imageView = view;
		imageInfo.sampler = sampler;
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_set;
		write.dstBinding = binding;
		write.dstArrayElement = arrayElement;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.pImageInfo = &imageInfo;
		vkUpdateDescriptorSets(m_owner->vkDevice(), 1, &write, 0, nullptr);
	}

	void DescriptorSet::writeBuffer(uint32_t binding, uint32_t arrayElement, VkDescriptorType type, VkBuffer buffer, VkDeviceSize range) const
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = buffer;
		bufferInfo.range = range;
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_set;
		write.dstBinding = binding;
		write.dstArrayElement = arrayElement;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.pBufferInfo = &bufferInfo;
		vkUpdateDescriptorSets(m_owner->vkDevice(), 1, &write, 0, nullptr);
	}

	acm::Error DescriptorSet::fail(const char* call, const std::string& reason)
	{
		return acm::Error(std::string(call) + ": " + reason);
	}

} // namespace acm::vulkan
