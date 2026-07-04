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

	Surface Instance::createHeadlessSurface(Extent2D extent)
	{
		return m ? m->createHeadlessSurface(extent) : Surface{};
	}

	Device Instance::createDevice(const DeviceInfo& deviceInfo, uint32_t queueFamily)
	{
		return m ? m->createDevice(deviceInfo, queueFamily) : Device{};
	}

	Device Instance::createDevice(const DeviceOption& option)
	{
		const std::vector<DeviceInfo>& available = devices();
		if (option.deviceIndex >= available.size())
			return Device{};
		return createDevice(available[option.deviceIndex], option.queueFamily);
	}

	// -----------------------------------------------------------------------------
	// Enumeration
	// -----------------------------------------------------------------------------

	const std::vector<DeviceInfo>& Instance::devices() const
	{
		static const std::vector<DeviceInfo> empty;
		return m ? m->devices() : empty;
	}

	std::vector<DeviceOption> Instance::deviceOptions() const
	{
		std::vector<DeviceOption> options;
		for (const DeviceInfo& deviceInfo : devices())
			for (const QueueInfo& queue : deviceInfo.queues)
				if (queue.graphics)
					options.push_back({deviceInfo.index, queue.family});
		return options;
	}

	std::vector<SurfaceOption> Instance::surfaceOptions(const Surface& surface) const
	{
		return surfaceOptions(std::vector<Surface>{surface});
	}

	std::vector<SurfaceOption> Instance::surfaceOptions(const std::vector<Surface>& surfaces) const
	{
		std::vector<SurfaceOption> options;
		if (surfaces.empty())
			return options;

		const auto sameFormat = [](const SurfaceFormat& a, const SurfaceFormat& b)
		{
			return a.format == b.format && a.colorSpace == b.colorSpace;
		};
		const auto hasFormat = [&sameFormat](const SurfaceDeviceSupport& support, const SurfaceFormat& format)
		{
			for (const SurfaceFormat& candidate : support.formats)
				if (sameFormat(candidate, format))
					return true;
			return false;
		};
		const auto hasPresentMode = [](const SurfaceDeviceSupport& support, PresentMode presentMode)
		{
			for (PresentMode candidate : support.presentModes)
				if (candidate == presentMode)
					return true;
			return false;
		};
		const auto preferredPresentMode = [](const SurfaceDeviceSupport& support)
		{
			for (PresentMode mode : support.presentModes)
				if (mode == PresentMode::Fifo)
					return mode;
			return support.presentModes[0];
		};

		for (const DeviceOption& deviceOption : deviceOptions())
		{
			SurfaceOption option;
			option.device = deviceOption;
			bool compatible = true;
			for (size_t surfaceIndex = 0; surfaceIndex < surfaces.size(); ++surfaceIndex)
			{
				const std::vector<SurfaceDeviceSupport>& supportList = surfaces[surfaceIndex].deviceSupport();
				const SurfaceDeviceSupport* support = nullptr;
				for (const SurfaceDeviceSupport& candidate : supportList)
					if (candidate.deviceIndex == deviceOption.deviceIndex)
					{
						support = &candidate;
						break;
					}
				if (!support || support->formats.empty() || support->presentModes.empty() || deviceOption.queueFamily >= support->queuePresentSupport.size() || !support->queuePresentSupport[deviceOption.queueFamily])
				{
					compatible = false;
					break;
				}

				if (surfaceIndex == 0)
				{
					option.format = support->formats[0];
					option.presentMode = preferredPresentMode(*support);
				}
				else if (!hasFormat(*support, option.format) || !hasPresentMode(*support, option.presentMode))
				{
					compatible = false;
					break;
				}
			}
			if (compatible)
				options.push_back(option);
		}
		return options;
	}

} // namespace acm
