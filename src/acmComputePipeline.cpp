#include "archimedes/acmComputePipeline.h"

#include "archimedes/nativeAPI.h"

#include <utility>

acm::ComputePipeline::ComputePipeline() = default;

acm::ComputePipeline::ComputePipeline(acm::ResourceRef<acm::native::ComputePipeline> resource)
	: m_resource(std::move(resource))
{
}

acm::ComputePipeline::ComputePipeline(acm::Error error)
	: m_error(std::move(error))
{
}

acm::ComputePipeline::ComputePipeline(const acm::ComputePipeline& other) = default;

acm::ComputePipeline& acm::ComputePipeline::operator=(const acm::ComputePipeline& other) = default;

acm::ComputePipeline::ComputePipeline(acm::ComputePipeline&& other) noexcept = default;

acm::ComputePipeline& acm::ComputePipeline::operator=(acm::ComputePipeline&& other) noexcept = default;

acm::ComputePipeline::~ComputePipeline() = default;

void acm::ComputePipeline::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::ComputePipeline::valid() const
{
	return m_resource.valid();
}

acm::Error acm::ComputePipeline::error() const
{
	return m_error;
}

acm::native::ComputePipeline* acm::ComputePipeline::native() const
{
	return m_resource.access();
}
