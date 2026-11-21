/*
 *  Created by LuckyNeko on 21/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

// Internal (not installed) Vulkan memory helpers. Included only by library .cpp
// files, which already pull in <vulkan/vulkan.h>.

#include <vulkan/vulkan.h>

#include <memory>
#include <mutex>
#include <vector>

namespace acm::vulkan
{
	// A sub-range of a pooled VkDeviceMemory block. `memory`/`offset` are what you
	// bind a resource to; `mapped` is a persistent CPU pointer to this range for
	// host-visible blocks (null otherwise — no per-resource vkMapMemory). Free it
	// back to the pool with MemoryAllocator::free.
	struct Allocation
	{
		VkDeviceMemory memory{VK_NULL_HANDLE};
		VkDeviceSize offset{0};
		VkDeviceSize size{0};
		void* mapped{nullptr};
		void* block{nullptr}; // owning block, opaque to callers (used by free)

		bool valid() const { return memory != VK_NULL_HANDLE; }
	};

	// What an allocation backs. Vulkan forbids a linear resource (a buffer) and a
	// non-linear one (an optimal-tiling image) from sharing a
	// VkPhysicalDeviceLimits::bufferImageGranularity page, so the allocator keeps
	// the two kinds in separate blocks and needs no per-allocation padding.
	enum class AllocationKind
	{
		Linear,	   // buffers
		NonLinear, // optimal-tiling images
	};

	// A simple pooling sub-allocator: it carves resource allocations out of large
	// VkDeviceMemory blocks (one block serves many resources), instead of one
	// vkAllocateMemory per resource — which keeps us well under
	// maxMemoryAllocationCount and avoids per-resource allocation overhead. Each
	// block holds one memory type and one AllocationKind. Per block it keeps a
	// first-fit free list (coalesced on free). Blocks of a host-visible memory type
	// are persistently mapped. Allocation/free are protected by an allocator mutex;
	// one instance lives per Device. Blocks are freed when the allocator is destroyed,
	// so every allocation must be free()d (and its resource destroyed) first.
	class MemoryAllocator
	{
	public:
		// Lifetime
		MemoryAllocator(VkDevice device, VkPhysicalDevice physicalDevice);
		~MemoryAllocator();

		// Allocation
		// Sub-allocates for `req` from a block whose memory type satisfies `props`
		// and whose kind is `kind` (a new block is created if none fits). Images use
		// the default; buffers pass AllocationKind::Linear. Returns an invalid
		// Allocation on failure, including a failed persistent map of a new block.
		Allocation allocate(const VkMemoryRequirements& req, VkMemoryPropertyFlags props, AllocationKind kind = AllocationKind::NonLinear);
		void free(const Allocation& allocation);

		// Diagnostics
		size_t blockCount() const; // number of live VkDeviceMemory blocks (for tests/diagnostics)

	private:
		// Internals
		struct Block;
		static constexpr VkDeviceSize DefaultBlockSize = 64ull * 1024 * 1024;
		static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment);
		uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

		VkDevice m_device{VK_NULL_HANDLE};
		VkPhysicalDeviceMemoryProperties m_memoryProperties{};
		mutable std::mutex m_mutex;
		std::vector<std::unique_ptr<Block>> m_blocks;
	};
} // namespace acm::vulkan
