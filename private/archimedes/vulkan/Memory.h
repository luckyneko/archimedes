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

	// A simple pooling sub-allocator: it carves resource allocations out of large
	// VkDeviceMemory blocks (one block serves many resources), instead of one
	// vkAllocateMemory per resource — which keeps us well under
	// maxMemoryAllocationCount and avoids per-resource allocation overhead. Per
	// block it keeps a first-fit free list (coalesced on free). Host-visible blocks
	// are persistently mapped. Allocation/free are protected by an allocator mutex;
	// one instance lives per Device. Blocks are freed when the allocator is destroyed,
	// so every allocation must be free()d (and its resource destroyed) first.
	class MemoryAllocator
	{
	public:
		MemoryAllocator(VkDevice device, VkPhysicalDevice physicalDevice);
		~MemoryAllocator();

		// Sub-allocates for `req` from a block whose memory type satisfies `props`
		// (a new block is created if none fits). Returns an invalid Allocation on
		// failure.
		Allocation allocate(const VkMemoryRequirements& req, VkMemoryPropertyFlags props);
		void free(const Allocation& allocation);

		size_t blockCount() const; // number of live VkDeviceMemory blocks (for tests/diagnostics)

	private:
		struct Block;
		static constexpr VkDeviceSize DefaultBlockSize = 64ull * 1024 * 1024;
		static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment);
		static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeBits, VkMemoryPropertyFlags properties);

		VkDevice m_device{VK_NULL_HANDLE};
		VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
		mutable std::mutex m_mutex;
		std::vector<std::unique_ptr<Block>> m_blocks;
	};
} // namespace acm::vulkan
