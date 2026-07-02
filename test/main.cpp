#include "archimedes/vulkan/RuntimeEnv.h"

#include <catch2/catch_all.hpp>

int main(int argc, char* argv[])
{
	acm::vulkan::useStagedVulkanICD();
	return Catch::Session().run(argc, argv);
}
