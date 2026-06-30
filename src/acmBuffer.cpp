#include "archimedes/acmBuffer.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Buffer::Buffer() = default;

acm::Buffer::Buffer(acm::ResourceRef<acm::native::Buffer> resource)
	: m_resource(std::move(resource))
{
}

acm::Buffer::Buffer(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Buffer::Buffer(const acm::Buffer& other) = default;

acm::Buffer& acm::Buffer::operator=(const acm::Buffer& other) = default;

acm::Buffer::Buffer(acm::Buffer&& other) noexcept = default;

acm::Buffer& acm::Buffer::operator=(acm::Buffer&& other) noexcept = default;

acm::Buffer::~Buffer() = default;

void acm::Buffer::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::Buffer::valid() const
{
	return m_resource.valid();
}

acm::Error acm::Buffer::error() const
{
	return m_error;
}

acm::native::Buffer* acm::Buffer::native() const
{
	return m_resource.access();
}

size_t acm::Buffer::size() const
{
	if (auto* resource = m_resource.access())
		return resource->size();
	return 0;
}

void* acm::Buffer::map()
{
	if (auto* resource = m_resource.access())
		return resource->map();
	return nullptr;
}

void acm::Buffer::unmap()
{
}

acm::Error acm::Buffer::write(const void* data, size_t size)
{
	if (!m_resource)
		return acm::Error("Buffer::write: invalid buffer");
	if (auto* resource = m_resource.access())
		return resource->write(data, size);
	return acm::Error("Buffer::write: invalid buffer");
}
