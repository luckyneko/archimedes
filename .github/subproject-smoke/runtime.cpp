#include <archimedes/archimedes.h>

#include <cstdio>

// A parent project's binary finds the CPU driver archimedes staged beside it.
int main()
{
	acm::useStagedVulkanRuntime();
	acm::Instance instance("parent-runtime-smoke", acm::Version{0, 1, 0});
	if (!instance.valid())
	{
		std::fprintf(stderr, "no Vulkan instance: %s\n", instance.error().message().c_str());
		return 1;
	}
	for (const acm::DeviceInfo& device : instance.devices())
		if (device.type == acm::PhysicalDeviceType::Cpu)
		{
			std::printf("CPU device: %s\n", device.name.c_str());
			return 0;
		}
	std::fprintf(stderr, "no CPU device\n");
	return 1;
}
