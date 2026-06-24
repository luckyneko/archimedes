#include "archimedes/vulkan/Memory.h"

#include <algorithm>

namespace acm::vulkan
{
	VkDeviceSize MemoryAllocator::alignUp(VkDeviceSize value, VkDeviceSize alignment)
	{
		if (alignment == 0)
			return value;
		return (value + alignment - 1) & ~(alignment - 1);
	}

	uint32_t MemoryAllocator::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeBits, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
		for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
		{
			const bool typeAllowed = (typeBits & (1u << index)) != 0;
			const bool hasProperties = (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
			if (typeAllowed && hasProperties)
				return index;
		}
		return UINT32_MAX;
	}

	// One VkDeviceMemory block, sub-divided by a free list of [offset,size) regions
	// kept sorted by offset (so neighbours can coalesce on free).
	struct MemoryAllocator::Block
	{
		VkDeviceMemory memory{VK_NULL_HANDLE};
		VkDeviceSize size{0};
		uint32_t memoryType{0};
		void* mapped{nullptr}; // persistent map for host-visible blocks
		struct Region
		{
			VkDeviceSize offset;
			VkDeviceSize size;
		};
		std::vector<Region> freeRegions;

		// First-fit: find a free region with room for `size` at `alignment`, carve
		// it out, and return its offset. False if nothing fits.
		bool tryAllocate(VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize& outOffset)
		{
			for (size_t i = 0; i < freeRegions.size(); ++i)
			{
				const VkDeviceSize regionEnd = freeRegions[i].offset + freeRegions[i].size;
				const VkDeviceSize aligned = MemoryAllocator::alignUp(freeRegions[i].offset, alignment);
				if (aligned + size > regionEnd)
					continue;

				const VkDeviceSize before = freeRegions[i].offset;
				const VkDeviceSize allocEnd = aligned + size;
				freeRegions.erase(freeRegions.begin() + i);
				// Re-insert the leftovers (alignment gap before, tail after), keeping
				// the list sorted by offset.
				if (allocEnd < regionEnd)
					freeRegions.insert(freeRegions.begin() + i, {allocEnd, regionEnd - allocEnd});
				if (aligned > before)
					freeRegions.insert(freeRegions.begin() + i, {before, aligned - before});
				outOffset = aligned;
				return true;
			}
			return false;
		}

		void freeRange(VkDeviceSize offset, VkDeviceSize size)
		{
			// Insert sorted by offset, then merge with any touching neighbours.
			size_t i = 0;
			while (i < freeRegions.size() && freeRegions[i].offset < offset)
				++i;
			freeRegions.insert(freeRegions.begin() + i, {offset, size});

			// Coalesce with previous, then with next.
			if (i > 0 && freeRegions[i - 1].offset + freeRegions[i - 1].size == freeRegions[i].offset)
			{
				freeRegions[i - 1].size += freeRegions[i].size;
				freeRegions.erase(freeRegions.begin() + i);
				--i;
			}
			if (i + 1 < freeRegions.size() && freeRegions[i].offset + freeRegions[i].size == freeRegions[i + 1].offset)
			{
				freeRegions[i].size += freeRegions[i + 1].size;
				freeRegions.erase(freeRegions.begin() + i + 1);
			}
		}
	};

	MemoryAllocator::MemoryAllocator(VkDevice device, VkPhysicalDevice physicalDevice)
		: m_device(device)
		, m_physicalDevice(physicalDevice)
	{
	}

	MemoryAllocator::~MemoryAllocator()
	{
		for (auto& block : m_blocks)
		{
			if (block->mapped)
				vkUnmapMemory(m_device, block->memory);
			vkFreeMemory(m_device, block->memory, nullptr);
		}
	}

	Allocation MemoryAllocator::allocate(const VkMemoryRequirements& req, VkMemoryPropertyFlags props)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const uint32_t memoryType = findMemoryType(m_physicalDevice, req.memoryTypeBits, props);
		if (memoryType == UINT32_MAX)
			return {};
		const bool hostVisible = (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;

		auto makeAllocation = [&](Block* block, VkDeviceSize offset) -> Allocation
		{
			Allocation a;
			a.memory = block->memory;
			a.offset = offset;
			a.size = req.size;
			a.mapped = block->mapped ? static_cast<char*>(block->mapped) + offset : nullptr;
			a.block = block;
			return a;
		};

		// Reuse an existing block of the right memory type if it has room.
		for (auto& block : m_blocks)
		{
			if (block->memoryType != memoryType)
				continue;
			VkDeviceSize offset = 0;
			if (block->tryAllocate(req.size, req.alignment, offset))
				return makeAllocation(block.get(), offset);
		}

		// None fit: create a new block (dedicated + exactly sized if the request is
		// larger than the default block size).
		const VkDeviceSize blockSize = std::max(DefaultBlockSize, req.size);
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = blockSize;
		allocInfo.memoryTypeIndex = memoryType;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
			return {};

		auto block = std::make_unique<Block>();
		block->memory = memory;
		block->size = blockSize;
		block->memoryType = memoryType;
		if (hostVisible)
			vkMapMemory(m_device, memory, 0, VK_WHOLE_SIZE, 0, &block->mapped);
		block->freeRegions.push_back({0, blockSize});

		VkDeviceSize offset = 0;
		if (!block->tryAllocate(req.size, req.alignment, offset))
		{
			if (block->mapped)
				vkUnmapMemory(m_device, memory);
			vkFreeMemory(m_device, memory, nullptr);
			return {};
		}
		Block* raw = block.get();
		m_blocks.push_back(std::move(block));
		return makeAllocation(raw, offset);
	}

	void MemoryAllocator::free(const Allocation& allocation)
	{
		if (!allocation.valid() || !allocation.block)
			return;
		std::lock_guard<std::mutex> lock(m_mutex);
		static_cast<Block*>(allocation.block)->freeRange(allocation.offset, allocation.size);
	}

	size_t MemoryAllocator::blockCount() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_blocks.size();
	}
} // namespace acm::vulkan
