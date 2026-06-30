#include "archimedes/acmTexture.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Texture::Texture() = default;

acm::Texture::Texture(acm::ResourceRef<acm::native::Texture> resource)
	: m_resource(std::move(resource))
{
}

acm::Texture::Texture(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Texture::Texture(const acm::Texture& other) = default;

acm::Texture& acm::Texture::operator=(const acm::Texture& other) = default;

acm::Texture::Texture(acm::Texture&& other) noexcept = default;

acm::Texture& acm::Texture::operator=(acm::Texture&& other) noexcept = default;

acm::Texture::~Texture() = default;

void acm::Texture::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::Texture::valid() const
{
	return m_resource.valid();
}

acm::Error acm::Texture::error() const
{
	return m_error;
}

acm::native::Texture* acm::Texture::native() const
{
	return m_resource.access();
}

acm::Error acm::Texture::upload(const void* pixels, size_t size)
{
	if (!m_resource)
		return acm::Error("Texture::upload: invalid texture");
	if (auto* resource = m_resource.access())
		return resource->upload(pixels, size);
	return acm::Error("Texture::upload: invalid texture");
}

acm::Format acm::Texture::format() const
{
	if (auto* resource = m_resource.access())
		return resource->format();
	return acm::Format::Undefined;
}

acm::Extent2D acm::Texture::getExtent() const
{
	if (auto* resource = m_resource.access())
		return resource->extent();
	return acm::Extent2D{};
}

uint32_t acm::Texture::mipLevels() const
{
	if (auto* resource = m_resource.access())
		return resource->mipLevels();
	return 0;
}
