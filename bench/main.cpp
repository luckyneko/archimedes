/*
 *  Created by LuckyNeko on 02/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/RuntimeEnv.h"

#include <catch2/catch_all.hpp>

int main(int argc, char* argv[])
{
	acm::vulkan::useStagedVulkanICD();
	return Catch::Session().run(argc, argv);
}
