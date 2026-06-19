#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmVkFwd.h"

namespace acm
{
	class CommandPool
	{
	public:
		CommandPool() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		// Allocates one primary command buffer from the pool. The pool is created
		// with RESET_COMMAND_BUFFER, so each buffer can be re-recorded every frame
		// (CommandBuffer::begin implicitly resets it).
		acm::CommandBuffer allocate();

		VkCommandPool vkCommandPool() const;

	private:
		friend class Device; // only Device::createCommandPool builds one
		CommandPool(acm::Device device);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
