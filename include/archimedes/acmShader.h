#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"
#include "archimedes/acmNative.h"

#include <vector>

namespace acm
{
	class Shader
	{
	public:
		Shader();
		Shader(const acm::Shader& other);
		Shader& operator=(const acm::Shader& other);
		Shader(acm::Shader&& other) noexcept;
		Shader& operator=(acm::Shader&& other) noexcept;
		~Shader();

		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::native::Shader* native() const;

	private:
		friend acm::native::Device;
		// spirv is the compiled SPIR-V bytecode (loading it from disk is the
		// caller's concern). codeSize is in bytes; the data must be 4-byte aligned,
		// which std::vector already guarantees.
		Shader(acm::ResourceRef<acm::native::Shader> resource);
		explicit Shader(acm::Error error);

		acm::ResourceRef<acm::native::Shader> m_resource;
		acm::Error m_error;
	};
} // namespace acm
