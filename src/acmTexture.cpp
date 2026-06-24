#include "archimedes/acmTexture.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Texture::Texture() = default;

acm::Texture::Texture(acm::native::Texture* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::Texture::Texture(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Texture::Texture(const acm::Texture& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_handle.valid())
		m_resource->retain(m_handle);
}

acm::Texture& acm::Texture::operator=(const acm::Texture& other)
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

acm::Texture::Texture(acm::Texture&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::Texture& acm::Texture::operator=(acm::Texture&& other) noexcept
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

acm::Texture::~Texture()
{
	reset();
}

void acm::Texture::reset()
{
	if (m_handle.valid())
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::Texture::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::Texture::error() const
{
	return m_error;
}

acm::Error acm::Texture::upload(const void* pixels, size_t size)
{
	if (!m_resource)
		return acm::Error("Texture::upload: invalid texture");
	return m_resource->upload(m_handle, pixels, size);
}

acm::Format acm::Texture::format() const
{
	return m_resource ? m_resource->format(m_handle) : acm::Format::Undefined;
}

acm::Extent2D acm::Texture::getExtent() const
{
	return m_resource ? m_resource->extent(m_handle) : acm::Extent2D{};
}

uint32_t acm::Texture::mipLevels() const
{
	return m_resource ? m_resource->mipLevels(m_handle) : 0;
}
