#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmNative.h"
#include "archimedes/acmTypes.h"

#include <cstddef>

namespace acm
{
	class SwapChain
	{
	public:
		SwapChain();
		SwapChain(const acm::SwapChain& other);
		SwapChain& operator=(const acm::SwapChain& other);
		SwapChain(acm::SwapChain&& other) noexcept;
		SwapChain& operator=(acm::SwapChain&& other) noexcept;
		~SwapChain();

		void reset();
		bool valid() const;
		acm::Error error() const;
		const acm::Handle& handle() const { return m_handle; }
		acm::native::SwapChain* native() const { return m_resource; }

		bool recreate();
		acm::SurfaceFormat getFormat() const;
		acm::Extent2D getExtents() const;
		size_t getRenderTargetCount() const;
		acm::RenderTarget getRenderTarget(size_t idx) const;

	private:
		friend acm::native::Device;
		SwapChain(acm::native::SwapChain* resource, acm::Handle handle);
		explicit SwapChain(acm::Error error);

		acm::native::SwapChain* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
