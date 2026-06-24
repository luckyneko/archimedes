#include "archimedes/acmSampler.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Sampler::Sampler() = default;

acm::Sampler::Sampler(acm::native::Sampler* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::Sampler::Sampler(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Sampler::Sampler(const acm::Sampler& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::Sampler& acm::Sampler::operator=(const acm::Sampler& other)
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

acm::Sampler::Sampler(acm::Sampler&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::Sampler& acm::Sampler::operator=(acm::Sampler&& other) noexcept
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

acm::Sampler::~Sampler()
{
	reset();
}

void acm::Sampler::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::Sampler::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::Sampler::error() const
{
	return m_error;
}
