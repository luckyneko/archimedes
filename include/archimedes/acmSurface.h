/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmResourceRef.h"
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
		acm::native::Surface* native() const;

		const std::vector<acm::GPUSurfaceSupport>& getGPUSupport() const;

	private:
		friend acm::native::Instance;
		Surface(acm::ResourceRef<acm::native::Surface> resource);
		explicit Surface(acm::Error error);

		acm::ResourceRef<acm::native::Surface> m_resource;
		acm::Error m_error;
	};
} // namespace acm
