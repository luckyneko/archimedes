/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmBackend.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmDeviceInfo.h"
#include "archimedes/acmResourceRef.h"

namespace acm
{
	// Copyable handle to a platform window surface created by Instance. Surface support
	// is captured per enumerated physical device so callers can pick a present-capable queue.
	class Surface
	{
	public:
		// Lifetime
		Surface();
		Surface(const acm::Surface& other);
		Surface& operator=(const acm::Surface& other);
		Surface(acm::Surface&& other) noexcept;
		Surface& operator=(acm::Surface&& other) noexcept;
		~Surface();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::backend::Surface* backend() const;

	private:
		// Construction
		friend acm::backend::Instance;
		Surface(acm::ResourceRef<acm::backend::Surface> resource);
		explicit Surface(acm::Error error);

		acm::ResourceRef<acm::backend::Surface> m_resource;
		acm::Error m_error;
	};
} // namespace acm
