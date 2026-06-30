#include "archimedes/acmShader.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Shader::Shader() = default;

acm::Shader::Shader(acm::ResourceRef<acm::native::Shader> resource)
	: m_resource(std::move(resource))
{
}

acm::Shader::Shader(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Shader::Shader(const acm::Shader& other) = default;

acm::Shader& acm::Shader::operator=(const acm::Shader& other) = default;

acm::Shader::Shader(acm::Shader&& other) noexcept = default;

acm::Shader& acm::Shader::operator=(acm::Shader&& other) noexcept = default;

acm::Shader::~Shader() = default;

void acm::Shader::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::Shader::valid() const
{
	return m_resource.valid();
}

acm::Error acm::Shader::error() const
{
	return m_error;
}

acm::native::Shader* acm::Shader::native() const
{
	return m_resource.access();
}
