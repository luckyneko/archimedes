#include "archimedes/acmCommandPool.h"

#include "archimedes/acmCommandBuffer.h"
#include "archimedes/nativeAPI.h"

#include <utility>

acm::CommandPool::CommandPool() = default;

acm::CommandPool::CommandPool(acm::native::CommandPool* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::CommandPool::CommandPool(acm::Error error)
	: m_error(std::move(error))
{
}

acm::CommandPool::CommandPool(const acm::CommandPool& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::CommandPool& acm::CommandPool::operator=(const acm::CommandPool& other)
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

acm::CommandPool::CommandPool(acm::CommandPool&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::CommandPool& acm::CommandPool::operator=(acm::CommandPool&& other) noexcept
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

acm::CommandPool::~CommandPool()
{
	reset();
}

void acm::CommandPool::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::CommandPool::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::CommandPool::error() const
{
	return m_error;
}

acm::CommandBuffer acm::CommandPool::allocate()
{
	return m_resource ? m_resource->owner().allocateCommandBuffer(*this) : acm::CommandBuffer{};
}
