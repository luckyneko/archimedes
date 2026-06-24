#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmNative.h"

namespace acm
{
	class Surface
	{
	public:
		Surface();
		Surface(const acm::Surface& other);
		Surface& operator=(const acm::Surface& other);
		Surface(acm::Surface&& other) noexcept;
		Surface& operator=(acm::Surface&& other) noexcept;
		~Surface();

		void reset();
		bool valid() const;
		acm::Error error() const;
		const acm::Handle& handle() const { return m_handle; }
		acm::native::Surface* native() const { return m_resource; }

		const std::vector<acm::GPUSurfaceSupport>& getGPUSupport() const;

	private:
		friend acm::native::Instance;
		Surface(acm::native::Surface* resource, acm::Handle handle);
		explicit Surface(acm::Error error);

		acm::native::Surface* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
