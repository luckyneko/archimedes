#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmTypes.h"
#include "archimedes/HandleMap.h"
#include "archimedes/vulkan/Memory.h"

#include <vulkan/vulkan.h>

#include <cstddef>

namespace acm::vulkan
{
	class Device;

	class Buffer : public acm::ResourceSlot<acm::vulkan::Buffer, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, size_t size, acm::BufferUsage usage);
		size_t size(const acm::Handle& handle) const;
		void* map(const acm::Handle& handle);
		VkBuffer vkBuffer(const acm::Handle& handle) const;
		acm::Error write(const acm::Handle& handle, const void* data, size_t size);
		void retire(acm::vulkan::Device& owner);

	private:
		static bool isHostVisible(acm::BufferUsage usage);

		size_t m_size{0};
		bool m_hostVisible{true};
		VkBuffer m_buffer{VK_NULL_HANDLE};
		acm::vulkan::Allocation m_allocation;
	};
} // namespace acm::vulkan
