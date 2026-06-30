#include "archimedes/acmCommandPool.h"

#include "archimedes/acmCommandBuffer.h"
#include "archimedes/nativeAPI.h"

#include <utility>

acm::CommandPool::CommandPool() = default;

acm::CommandPool::CommandPool(acm::ResourceRef<acm::native::CommandPool> resource)
	: m_resource(std::move(resource))
{
}

acm::CommandPool::CommandPool(acm::Error error)
	: m_error(std::move(error))
{
}

acm::CommandPool::CommandPool(const acm::CommandPool& other) = default;

acm::CommandPool& acm::CommandPool::operator=(const acm::CommandPool& other) = default;

acm::CommandPool::CommandPool(acm::CommandPool&& other) noexcept = default;

acm::CommandPool& acm::CommandPool::operator=(acm::CommandPool&& other) noexcept = default;

acm::CommandPool::~CommandPool() = default;

void acm::CommandPool::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::CommandPool::valid() const
{
	return m_resource.valid();
}

acm::Error acm::CommandPool::error() const
{
	return m_error;
}

acm::native::CommandPool* acm::CommandPool::native() const
{
	return m_resource.access();
}

acm::CommandBuffer acm::CommandPool::allocate()
{
	if (auto* resource = m_resource.access())
		return resource->owner().allocateCommandBuffer(*this);
	return acm::CommandBuffer{};
}
