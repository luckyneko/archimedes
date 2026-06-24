#include "archimedes/acmPipeline.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Pipeline::Pipeline() = default;

acm::Pipeline::Pipeline(acm::native::Pipeline* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::Pipeline::Pipeline(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Pipeline::Pipeline(const acm::Pipeline& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::Pipeline& acm::Pipeline::operator=(const acm::Pipeline& other)
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

acm::Pipeline::Pipeline(acm::Pipeline&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::Pipeline& acm::Pipeline::operator=(acm::Pipeline&& other) noexcept
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

acm::Pipeline::~Pipeline()
{
	reset();
}

void acm::Pipeline::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::Pipeline::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::Pipeline::error() const
{
	return m_error;
}
