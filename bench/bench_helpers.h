/*
 *  Created by LuckyNeko on 02/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>

namespace acmbench
{
	struct DeviceStack
	{
		acm::Instance instance;
		acm::Device device;
	};

	inline bool buildGraphicsDevice(DeviceStack& out, const char* name)
	{
		out.instance = acm::Instance(name, acm::Version{0, 1, 0});
		if (!out.instance.valid())
		{
			SKIP("no Vulkan driver available");
			return false;
		}

		uint32_t queueIndex = 0;
		const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(out.instance, queueIndex);
		if (!gpu)
		{
			SKIP("no graphics-capable queue family");
			return false;
		}

		out.device = out.instance.createDevice(*gpu, queueIndex);
		REQUIRE(out.device.valid());
		return true;
	}
} // namespace acmbench
