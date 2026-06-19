#include <archimedes/archimedes.h>
#include <catch2/catch_all.hpp>
#include <vulkan/vulkan.h>

// Integration: needs a live Vulkan driver (vendored MoltenVK ICD via
// VK_ICD_FILENAMES). SKIPs when no driver is available.

TEST_CASE("Instance creates and enumerates GPUs", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	REQUIRE(instance.vkInstance() != VK_NULL_HANDLE);

	const auto& gpus = instance.getAvailableGPUs();
	REQUIRE_FALSE(gpus.empty());

	bool anyGraphics = false;
	for (uint32_t i = 0; i < gpus.size(); ++i)
	{
		const auto& gpu = gpus[i];
		REQUIRE(gpu.device != VK_NULL_HANDLE);
		REQUIRE(gpu.index == i);		 // index mirrors enumeration order
		REQUIRE_FALSE(gpu.name.empty()); // neutral GPU info populated
		REQUIRE_FALSE(gpu.queueFamilies.empty());
		for (const auto& qf : gpu.queueFamilies)
			anyGraphics = anyGraphics || qf.supportsGraphics;
	}
	REQUIRE(anyGraphics);
}

TEST_CASE("Instance is a shared handle", "[acm][gpu]")
{
	acm::Instance a("acm-tests", acm::Version{0, 1, 0, 0});
	if (!a.valid())
		SKIP("no Vulkan driver available");

	acm::Instance b = a; // shares the underlying VkInstance
	VkInstance raw = b.vkInstance();
	a.reset(); // drop one reference

	REQUIRE_FALSE(a.valid());
	REQUIRE(b.valid());
	REQUIRE(b.vkInstance() == raw); // still alive through b
}
