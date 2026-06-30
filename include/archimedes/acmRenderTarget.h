#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"
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
		acm::native::RenderTarget* native() const;

		acm::Extent2D getExtent() const;
		bool hasDepth() const;
		bool isMultisampled() const;

	private:
		friend acm::native::Device;
		RenderTarget(acm::ResourceRef<acm::native::RenderTarget> resource);
		explicit RenderTarget(acm::Error error);

		acm::ResourceRef<acm::native::RenderTarget> m_resource;
		acm::Error m_error;
	};
} // namespace acm
