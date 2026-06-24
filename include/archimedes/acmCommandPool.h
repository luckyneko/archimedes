#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmNative.h"

namespace acm
{
	class CommandPool
	{
	public:
		CommandPool();
		CommandPool(const acm::CommandPool& other);
		CommandPool& operator=(const acm::CommandPool& other);
		CommandPool(acm::CommandPool&& other) noexcept;
		CommandPool& operator=(acm::CommandPool&& other) noexcept;
		~CommandPool();

		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::CommandBuffer allocate();
		const acm::Handle& handle() const { return m_handle; }
		acm::native::CommandPool* native() const { return m_resource; }

	private:
		friend acm::native::Device;
		CommandPool(acm::native::CommandPool* resource, acm::Handle handle);
		explicit CommandPool(acm::Error error);

		acm::native::CommandPool* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
