#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmNative.h"

namespace acm
{
	// Owns a native sampler. Linear min/mag filtering, clamp-to-edge addressing, nearest
	// mipmap mode; optional anisotropic filtering (createSampler's maxAnisotropy > 1,
	// gated on the samplerAnisotropy device feature and clamped to its limit). One
	// sampler is reusable across many textures/descriptor sets.
	class Sampler
	{
	public:
		Sampler();
		Sampler(const acm::Sampler& other);
		Sampler& operator=(const acm::Sampler& other);
		Sampler(acm::Sampler&& other) noexcept;
		Sampler& operator=(acm::Sampler&& other) noexcept;
		~Sampler();

		void reset();
		bool valid() const;
		acm::Error error() const;
		const acm::Handle& handle() const { return m_handle; }
		acm::native::Sampler* native() const { return m_resource; }

	private:
		friend acm::native::Device;
		Sampler(acm::native::Sampler* resource, acm::Handle handle);
		explicit Sampler(acm::Error error);

		acm::native::Sampler* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
