#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmVkFwd.h"
#include <vector>

namespace acm
{
	class Shader
	{
	public:
		Shader() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		VkShaderModule vkShaderModule() const;

	private:
		friend class Device; // only Device::createShader builds one
		// spirv is the compiled SPIR-V bytecode (loading it from disk is the
		// caller's concern). codeSize is in bytes; the data must be 4-byte aligned,
		// which std::vector already guarantees.
		Shader(acm::Device device, const std::vector<char>& spirv);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
