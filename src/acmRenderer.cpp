#include "archimedes/acmRenderer.h"

#include "archimedes/acmCommandBuffer.h"
#include "archimedes/nativeAPI.h"

#include <utility>

acm::Renderer::Renderer() = default;

acm::Renderer::Renderer(acm::native::Renderer* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::Renderer::Renderer(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Renderer::Renderer(const acm::Renderer& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::Renderer& acm::Renderer::operator=(const acm::Renderer& other)
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

acm::Renderer::Renderer(acm::Renderer&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::Renderer& acm::Renderer::operator=(acm::Renderer&& other) noexcept
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

acm::Renderer::~Renderer()
{
	reset();
}

void acm::Renderer::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::Renderer::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::Renderer::error() const
{
	return m_error;
}

acm::Error acm::Renderer::render(const std::function<void(acm::CommandBuffer&, uint32_t)>& record)
{
	return m_resource ? m_resource->render(m_handle, {}, record) : acm::Error("invalid renderer");
}

acm::Error acm::Renderer::render(const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record)
{
	return m_resource ? m_resource->render(m_handle, prePass, record) : acm::Error("invalid renderer");
}
