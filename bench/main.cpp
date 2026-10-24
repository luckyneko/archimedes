/*
 *  Created by LuckyNeko on 02/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmRuntime.h"
#include "archimedes/archimedes.h"

#include <catch2/catch_all.hpp>
#include <cstdio>
#include <vector>

namespace
{
	// Prints "<name>\t<hardware|cpu>" for the top-ranked graphics device — the one the
	// benchmarks run on — so bench/report.py can record it beside the results.
	int printDeviceInfo()
	{
		acm::Instance instance("acm-bench", acm::Version{0, 1, 0});
		if (!instance.valid())
			return 1;
		const std::vector<acm::DeviceOption> options = instance.graphicsOptions();
		if (options.empty())
			return 1;
		const acm::DeviceInfo& device = instance.devices()[options.front().deviceIndex];
		std::printf("%s\t%s\n", device.name.c_str(), device.type == acm::PhysicalDeviceType::Cpu ? "cpu" : "hardware");
		return 0;
	}
} // namespace

int main(int argc, char* argv[])
{
	acm::useStagedVulkanRuntime();

	Catch::Session session;
	bool deviceInfo = false;
	session.cli(session.cli() | Catch::Clara::Opt(deviceInfo)["--device-info"]("print the device the benchmarks run on, then exit"));
	if (const int result = session.applyCommandLine(argc, argv); result != 0)
		return result;
	if (deviceInfo)
		return printDeviceInfo();
	return session.run();
}
