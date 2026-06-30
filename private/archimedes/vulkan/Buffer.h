#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmTypes.h"
#include "archimedes/vulkan/Memory.h"

#include <vulkan/vulkan.h>

#include <cstddef>

namespace acm::vulkan
{
	class Device;

	class Buffer
	{
	public:
		bool create(acm::vulkan::Device& owner, size_t size, acm::BufferUsage usage);
		acm::vulkan::Device& owner() const { return *m_owner; }
		size_t size() const;
		void* map();
		VkBuffer vkBuffer() const;
		acm::Error write(const void* data, size_t size);
		void retire(acm::vulkan::Device& owner);

	private:
		static bool isHostVisible(acm::BufferUsage usage);

		acm::vulkan::Device* m_owner{nullptr};
		size_t m_size{0};
		bool m_hostVisible{true};
		VkBuffer m_buffer{VK_NULL_HANDLE};
		acm::vulkan::Allocation m_allocation;
	};
} // namespace acm::vulkan
