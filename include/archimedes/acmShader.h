#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
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
		const acm::Handle& handle() const { return m_handle; }
		acm::native::Shader* native() const { return m_resource; }

	private:
		friend acm::native::Device;
		// spirv is the compiled SPIR-V bytecode (loading it from disk is the
		// caller's concern). codeSize is in bytes; the data must be 4-byte aligned,
		// which std::vector already guarantees.
		Shader(acm::native::Shader* resource, acm::Handle handle);
		explicit Shader(acm::Error error);

		acm::native::Shader* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
