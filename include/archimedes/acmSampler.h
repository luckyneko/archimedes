#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmVkFwd.h"

namespace acm
{
	// Owns a VkSampler. Linear min/mag filtering, clamp-to-edge addressing, nearest
	// mipmap mode; optional anisotropic filtering (createSampler's maxAnisotropy > 1,
	// gated on the samplerAnisotropy device feature and clamped to its limit). One
	// sampler is reusable across many textures/descriptor sets.
	class Sampler
	{
	public:
		Sampler() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		VkSampler vkSampler() const;

	private:
		friend class Device; // only Device::createSampler builds one
		Sampler(acm::Device device, float maxAnisotropy);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
