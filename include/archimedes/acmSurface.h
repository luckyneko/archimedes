#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmVkFwd.h"

namespace acm
{
	class Surface
	{
	public:
		Surface() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		const std::vector<acm::GPUSurfaceSupport>& getGPUSupport() const;
		VkSurfaceKHR vkSurface();

	private:
		friend class Instance; // only Instance::createSurface builds one
		Surface(acm::Instance instance, VkSurfaceKHR surface);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm