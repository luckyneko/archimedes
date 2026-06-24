#include "archimedes/acmBuffer.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Buffer::Buffer() = default;

acm::Buffer::Buffer(acm::native::Buffer* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::Buffer::Buffer(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Buffer::Buffer(const acm::Buffer& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::Buffer& acm::Buffer::operator=(const acm::Buffer& other)
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

acm::Buffer::Buffer(acm::Buffer&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::Buffer& acm::Buffer::operator=(acm::Buffer&& other) noexcept
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

acm::Buffer::~Buffer()
{
	reset();
}

void acm::Buffer::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::Buffer::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::Buffer::error() const
{
	return m_error;
}

size_t acm::Buffer::size() const
{
	return m_resource ? m_resource->size(m_handle) : 0;
}

void* acm::Buffer::map()
{
	return m_resource ? m_resource->map(m_handle) : nullptr;
}

void acm::Buffer::unmap()
{
}

acm::Error acm::Buffer::write(const void* data, size_t size)
{
	if (!m_resource)
		return acm::Error("Buffer::write: invalid buffer");
	return m_resource->write(m_handle, data, size);
}
