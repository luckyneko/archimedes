#include "archimedes/acmInstance.h"

#include "archimedes/acmDevice.h"
#include "archimedes/acmSurface.h"
#include "archimedes/nativeAPI.h"

#include <utility>

acm::Instance::Instance() = default;

acm::Instance::Instance(const char* appName, const acm::Version& appVer, const acm::InstanceConfig& config)
{
	auto instance = std::make_unique<acm::native::Instance>(appName, appVer, config);
	if (!instance->valid())
	{
		m_error = instance->error();
		return;
	}
	m = std::move(instance);
}

acm::Instance::Instance(acm::Instance&& other) noexcept = default;

acm::Instance& acm::Instance::operator=(acm::Instance&& other) noexcept = default;

acm::Instance::~Instance() = default;

void acm::Instance::reset()
{
	m.reset();
	m_error = {};
}

bool acm::Instance::valid() const
{
	return m && m->valid();
}

acm::Error acm::Instance::error() const
{
	return m_error;
}

acm::Surface acm::Instance::createSurface(acm::native::SurfaceHandle surface)
{
	return m ? m->createSurface(surface) : acm::Surface{};
}

acm::Device acm::Instance::createDevice(const acm::GPU& gpu, uint32_t queueIdx)
{
	return m ? m->createDevice(gpu, queueIdx) : acm::Device{};
}

const std::vector<acm::GPU>& acm::Instance::getAvailableGPUs() const
{
	static const std::vector<acm::GPU> empty;
	return m ? m->gpus() : empty;
}

acm::native::InstanceHandle acm::Instance::nativeInstance() const
{
	return m ? m->nativeInstance() : acm::native::InstanceHandle{};
}
