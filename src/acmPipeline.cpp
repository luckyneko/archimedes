#include "archimedes/acmPipeline.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::Pipeline::Pipeline() = default;

acm::Pipeline::Pipeline(acm::ResourceRef<acm::native::Pipeline> resource)
	: m_resource(std::move(resource))
{
}

acm::Pipeline::Pipeline(acm::Error error)
	: m_error(std::move(error))
{
}

acm::Pipeline::Pipeline(const acm::Pipeline& other) = default;

acm::Pipeline& acm::Pipeline::operator=(const acm::Pipeline& other) = default;

acm::Pipeline::Pipeline(acm::Pipeline&& other) noexcept = default;

acm::Pipeline& acm::Pipeline::operator=(acm::Pipeline&& other) noexcept = default;

acm::Pipeline::~Pipeline() = default;

void acm::Pipeline::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::Pipeline::valid() const
{
	return m_resource.valid();
}

acm::Error acm::Pipeline::error() const
{
	return m_error;
}

acm::native::Pipeline* acm::Pipeline::native() const
{
	return m_resource.access();
}
