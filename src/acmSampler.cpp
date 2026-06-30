#include "archimedes/acmSampler.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Sampler::Sampler() = default;

acm::Sampler::Sampler(acm::ResourceRef<acm::native::Sampler> resource)
	: m_resource(std::move(resource))
{
}

acm::Sampler::Sampler(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Sampler::Sampler(const acm::Sampler& other) = default;

acm::Sampler& acm::Sampler::operator=(const acm::Sampler& other) = default;

acm::Sampler::Sampler(acm::Sampler&& other) noexcept = default;

acm::Sampler& acm::Sampler::operator=(acm::Sampler&& other) noexcept = default;

acm::Sampler::~Sampler() = default;

void acm::Sampler::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::Sampler::valid() const
{
	return m_resource.valid();
}

acm::Error acm::Sampler::error() const
{
	return m_error;
}

acm::native::Sampler* acm::Sampler::native() const
{
	return m_resource.access();
}
