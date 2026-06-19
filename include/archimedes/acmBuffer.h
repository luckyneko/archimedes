#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"
#include <cstddef>
#include <cstdint>

namespace acm
{
	// An owned VkBuffer whose memory heap is chosen by its BufferUsage: vertex/index
	// buffers are device-local (written once, read many — filled via a staging copy);
	// uniform and readback buffers are host-visible + coherent (CPU-mappable, no flush
	// needed). write() handles both — a direct memcpy for host-visible, a synchronous
	// staging upload for device-local — while map()/unmap() work on host-visible
	// buffers only.
	class Buffer
	{
	public:
		Buffer() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		size_t size() const;
		void* map(); // host-visible buffers only; returns a pointer to the (coherent) mapping (else nullptr)
		void unmap();
		// Uploads up to size() bytes. Host-visible: map + memcpy + unmap. Device-local:
		// a staging buffer + one-shot copy that waits idle, so it is a load-time call.
		acm::Error write(const void* data, size_t size);
		VkBuffer vkBuffer() const;

	private:
		friend class Device; // only Device::createBuffer builds one
		Buffer(acm::Device device, size_t size, acm::BufferUsage usage);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
