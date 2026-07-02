/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmInstance.h"

#include "archimedes/acmDevice.h"
#include "archimedes/acmSurface.h"
#include "archimedes/backendAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Instance::Instance() = default;

	Instance::Instance(const char* appName, const Version& appVer, const InstanceConfig& config)
	{
		auto instance = std::make_unique<backend::Instance>(appName, appVer, config);
		if (!instance->valid())
		{
			m_error = instance->error();
			return;
		}
		m = std::move(instance);
	}

	Instance::Instance(Instance&& other) noexcept = default;

	Instance& Instance::operator=(Instance&& other) noexcept = default;

	Instance::~Instance() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Instance::reset()
	{
		m.reset();
		m_error = {};
	}

	bool Instance::valid() const
	{
		return m && m->valid();
	}

	Error Instance::error() const
	{
		return m_error;
	}

	VkInstance Instance::vulkanInstance() const
	{
		return m ? m->vulkanInstance() : VK_NULL_HANDLE;
	}

	// -----------------------------------------------------------------------------
	// Factories
	// -----------------------------------------------------------------------------

	Surface Instance::createVulkanSurface(VkSurfaceKHR surface)
	{
		return m ? m->createVulkanSurface(surface) : Surface{};
	}

	Device Instance::createDevice(const GPU& gpu, uint32_t queueIndex)
	{
		return m ? m->createDevice(gpu, queueIndex) : Device{};
	}

	// -----------------------------------------------------------------------------
	// Enumeration
	// -----------------------------------------------------------------------------

	const std::vector<GPU>& Instance::getAvailableGPUs() const
	{
		static const std::vector<GPU> empty;
		return m ? m->gpus() : empty;
	}

} // namespace acm
