#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmNative.h"
#include "archimedes/acmTypes.h"

namespace acm
{
	class RenderTarget
	{
	public:
		RenderTarget();
		RenderTarget(const acm::RenderTarget& other);
		RenderTarget& operator=(const acm::RenderTarget& other);
		RenderTarget(acm::RenderTarget&& other) noexcept;
		RenderTarget& operator=(acm::RenderTarget&& other) noexcept;
		~RenderTarget();

		void reset();
		bool valid() const;
		acm::Error error() const;
		const acm::Handle& handle() const { return m_handle; }
		acm::native::RenderTarget* native() const { return m_resource; }

		acm::Extent2D getExtent() const;
		bool hasDepth() const;
		bool isMultisampled() const;

	private:
		friend acm::native::Device;
		RenderTarget(acm::native::RenderTarget* resource, acm::Handle handle);
		explicit RenderTarget(acm::Error error);

		acm::native::RenderTarget* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
