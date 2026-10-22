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

	// The top-ranked graphics device — the one an application would pick — so a run's
	// numbers belong to one device kind (bench/report.py records which).
	inline bool buildGraphicsDevice(DeviceStack& out, const char* name)
	{
		out.instance = acm::Instance(name, acm::Version{0, 1, 0});
		acmtest::requireInstance(out.instance);

		const acm::DeviceOption option = acmtest::selectGraphicsOption(out.instance, acmtest::topRankedKind(out.instance));
		out.device = out.instance.createDevice(option);
		REQUIRE(out.device.valid());
		return true;
	}
} // namespace acmbench
