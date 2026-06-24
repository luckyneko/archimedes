#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmNative.h"

namespace acm
{
	class ComputePipeline
	{
	public:
		ComputePipeline();
		ComputePipeline(const acm::ComputePipeline& other);
		ComputePipeline& operator=(const acm::ComputePipeline& other);
		ComputePipeline(acm::ComputePipeline&& other) noexcept;
		ComputePipeline& operator=(acm::ComputePipeline&& other) noexcept;
		~ComputePipeline();

		void reset();
		bool valid() const;
		acm::Error error() const;
		const acm::Handle& handle() const { return m_handle; }
		acm::native::ComputePipeline* native() const { return m_resource; }

	private:
		friend acm::native::Device;
		ComputePipeline(acm::native::ComputePipeline* resource, acm::Handle handle);
		explicit ComputePipeline(acm::Error error);

		acm::native::ComputePipeline* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
