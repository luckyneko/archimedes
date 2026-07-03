/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmBackend.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"

namespace acm
{
	// Copyable handle to a compute pipeline built from one compute shader and a
	// descriptor layout for the resources the dispatch reads or writes.
	class ComputePipeline
	{
	public:
		// Lifetime
		ComputePipeline();
		ComputePipeline(const acm::ComputePipeline& other);
		ComputePipeline& operator=(const acm::ComputePipeline& other);
		ComputePipeline(acm::ComputePipeline&& other) noexcept;
		ComputePipeline& operator=(acm::ComputePipeline&& other) noexcept;
		~ComputePipeline();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::backend::ComputePipeline* backend() const;

	private:
		// Construction
		friend acm::backend::Device;
		ComputePipeline(acm::ResourceRef<acm::backend::ComputePipeline> resource);
		explicit ComputePipeline(acm::Error error);

		acm::ResourceRef<acm::backend::ComputePipeline> m_resource;
		acm::Error m_error;
	};
} // namespace acm
