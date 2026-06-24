#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmNative.h"
#include "archimedes/acmTypes.h"

#include <cstddef>

namespace acm
{
	// An owned native buffer whose memory heap is chosen by its BufferUsage: vertex/index
	// buffers are device-local (written once, read many — filled via a staging copy);
	// uniform and readback buffers are host-visible + coherent (CPU-mappable, no flush
	// needed). write() handles both — a direct memcpy for host-visible, a synchronous
	// staging upload for device-local — while map()/unmap() work on host-visible
	// buffers only.
	class Buffer
	{
	public:
		Buffer();
		Buffer(const acm::Buffer& other);
		Buffer& operator=(const acm::Buffer& other);
		Buffer(acm::Buffer&& other) noexcept;
		Buffer& operator=(acm::Buffer&& other) noexcept;
		~Buffer();

		void reset();
		bool valid() const;
		acm::Error error() const;
		const acm::Handle& handle() const { return m_handle; }
		acm::native::Buffer* native() const { return m_resource; }

		size_t size() const;
		void* map(); // host-visible buffers only; returns a pointer to the (coherent) mapping (else nullptr)
		void unmap();
		// Uploads up to size() bytes. Host-visible: map + memcpy + unmap. Device-local:
		// a staging buffer + one-shot copy that waits idle, so it is a load-time call.
		acm::Error write(const void* data, size_t size);

	private:
		friend acm::native::Device;
		Buffer(acm::native::Buffer* resource, acm::Handle handle);
		explicit Buffer(acm::Error error);

		acm::native::Buffer* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
