#include "archimedes/acmComputePipeline.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::ComputePipeline::ComputePipeline() = default;

acm::ComputePipeline::ComputePipeline(acm::native::ComputePipeline* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::ComputePipeline::ComputePipeline(acm::Error error)
	: m_error(std::move(error))
{
}

acm::ComputePipeline::ComputePipeline(const acm::ComputePipeline& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::ComputePipeline& acm::ComputePipeline::operator=(const acm::ComputePipeline& other)
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

acm::ComputePipeline::ComputePipeline(acm::ComputePipeline&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::ComputePipeline& acm::ComputePipeline::operator=(acm::ComputePipeline&& other) noexcept
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

acm::ComputePipeline::~ComputePipeline()
{
	reset();
}

void acm::ComputePipeline::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::ComputePipeline::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::ComputePipeline::error() const
{
	return m_error;
}
