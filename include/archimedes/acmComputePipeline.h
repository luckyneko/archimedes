#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"
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
		acm::native::ComputePipeline* native() const;

	private:
		friend acm::native::Device;
		ComputePipeline(acm::ResourceRef<acm::native::ComputePipeline> resource);
		explicit ComputePipeline(acm::Error error);

		acm::ResourceRef<acm::native::ComputePipeline> m_resource;
		acm::Error m_error;
	};
} // namespace acm
