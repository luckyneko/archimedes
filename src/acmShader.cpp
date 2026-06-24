#include "archimedes/acmShader.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Shader::Shader() = default;

acm::Shader::Shader(acm::native::Shader* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::Shader::Shader(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Shader::Shader(const acm::Shader& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::Shader& acm::Shader::operator=(const acm::Shader& other)
{
	if (this == &other)
		return *this;
	reset();
	m_resource = other.m_resource;
	m_handle = other.m_handle;
	m_error = other.m_error;
	if (m_handle.valid())
		m_resource->retain(m_handle);
	return *this;
}

acm::Shader::Shader(acm::Shader&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::Shader& acm::Shader::operator=(acm::Shader&& other) noexcept
{
	if (this == &other)
		return *this;
	reset();
	m_resource = other.m_resource;
	m_handle = other.m_handle;
	m_error = std::move(other.m_error);
	other.m_resource = nullptr;
	other.m_handle.reset();
	return *this;
}

acm::Shader::~Shader()
{
	reset();
}

void acm::Shader::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::Shader::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::Shader::error() const
{
	return m_error;
}
