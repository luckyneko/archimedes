#include "archimedes/acmBuffer.h"

#include "archimedes/acmDevice.h"
#include "archimedes/acmVkConvert.h"
#include "archimedes/acmVkMemory.h"
#include "archimedes/acmVkOneShot.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstring>

namespace
{
	// Vertex/index data is written once and read many times by the GPU, so it lives in
	// device-local memory (filled via a staging copy). Uniforms (rewritten every frame),
	// readback targets, and staging scratch buffers stay host-visible so the CPU can
	// map them directly.
	bool isHostVisible(acm::BufferUsage usage)
	{
		return usage == acm::BufferUsage::Uniform || usage == acm::BufferUsage::TransferDst || usage == acm::BufferUsage::Staging || usage == acm::BufferUsage::Storage;
	}

	// Uploads `size` bytes into a device-local buffer through a host-visible Staging
	// buffer + a one-shot copy. Synchronous (oneShotSubmit waits the queue idle), so it
	// is a load-time operation, not a per-frame one.
	acm::Error stagedUpload(acm::Device device, VkBuffer dst, VkDeviceSize size, const void* data)
	{
		acm::Buffer staging = device.createBuffer(size_t(size), acm::BufferUsage::Staging);
		if (!staging.valid())
			return acm::Error("stagedUpload: failed to create staging buffer");
		if (auto err = staging.write(data, size_t(size)))
			return err;

		return acm::oneShotSubmit(device, [&staging, dst, size](VkCommandBuffer cb)
										  {
			VkBufferCopy region = {};
			region.size = size;
			vkCmdCopyBuffer(cb, staging.vkBuffer(), dst, 1, &region); });
	}
} // namespace

struct acm::Buffer::impl
{
	acm::Device device;
	size_t size{0};
	bool hostVisible{true};
	VkBuffer buffer{VK_NULL_HANDLE};
	acm::Allocation allocation; // sub-range of a pooled block

	~impl()
	{
		if (!device.valid())
			return;

		// Defer onto the device's frame-fenced queue: destroy the buffer, then return
		// its memory range to the pool. The allocator outlives the graveyard flush, so
		// capturing a raw pointer to it (not the Device handle) is safe and avoids a
		// shared_ptr cycle through the device's own graveyard.
		VkDevice dev = device.vkDevice();
		if (buffer)
		{
			VkBuffer b = buffer;
			device.enqueueDestroy([dev, b]
								  { vkDestroyBuffer(dev, b, nullptr); });
		}
		if (allocation.valid())
		{
			auto* alloc = &device.memoryAllocator();
			acm::Allocation a = allocation;
			device.enqueueDestroy([alloc, a]
								  { alloc->free(a); });
		}
	}
};

acm::Buffer::Buffer(acm::Device device, size_t size, acm::BufferUsage usage)
	: m()
{
	if (size == 0)
		return;

	auto impl = std::make_shared<acm::Buffer::impl>();
	impl->device = device;
	impl->size = size;
	impl->hostVisible = isHostVisible(usage);

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = acm::toVk(usage);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(impl->device.vkDevice(), &bufferInfo, nullptr, &impl->buffer) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create buffer");
		return;
	}

	// Host-visible (uniform/readback/staging) maps directly via the block's persistent
	// mapping; device-local (vertex/index) is filled via staging — toVk() gives those
	// usages a TRANSFER_DST flag for the copy.
	const VkMemoryPropertyFlags props = impl->hostVisible
											? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
											: VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(impl->device.vkDevice(), impl->buffer, &memReq);
	impl->allocation = impl->device.memoryAllocator().allocate(memReq, props);
	if (!impl->allocation.valid())
	{
		vkDestroyBuffer(impl->device.vkDevice(), impl->buffer, nullptr);
		impl->buffer = VK_NULL_HANDLE;
		m_error = acm::Error("failed to allocate buffer memory");
		return;
	}
	vkBindBufferMemory(impl->device.vkDevice(), impl->buffer, impl->allocation.memory, impl->allocation.offset);

	m = impl;
}

size_t acm::Buffer::size() const
{
	return m->size;
}

void* acm::Buffer::map()
{
	if (!m->hostVisible)
		return nullptr;
	// Host-visible blocks are persistently mapped; this range's pointer is precomputed.
	return m->allocation.mapped;
}

void acm::Buffer::unmap()
{
	// No-op: the backing block stays mapped for its lifetime (host-coherent, no flush).
}

acm::Error acm::Buffer::write(const void* data, size_t size)
{
	const size_t n = std::min(size, m->size);
	if (m->hostVisible)
	{
		void* dst = map();
		if (!dst)
			return acm::Error("Buffer::write: map failed");
		std::memcpy(dst, data, n);
		return acm::Error{};
	}
	else
	{
		return stagedUpload(m->device, m->buffer, n, data);
	}
}

VkBuffer acm::Buffer::vkBuffer() const
{
	return m->buffer;
}
