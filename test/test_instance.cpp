#include <archimedes/archimedes.h>
#include <vulkan/vulkan.h>

#include <catch2/catch_all.hpp>

// Integration: needs a live Vulkan driver (vendored MoltenVK ICD via
// VK_ICD_FILENAMES). SKIPs when no driver is available.

TEST_CASE("InstanceConfig has production-safe defaults", "[acm][unit]")
{
	constexpr acm::InstanceConfig config;
	STATIC_REQUIRE(config.portability);
	STATIC_REQUIRE_FALSE(config.validation);
	STATIC_REQUIRE_FALSE(config.debug);
}

TEST_CASE("Instance creates and enumerates GPUs", "[acm][gpu]")
{
	acm::InstanceConfig config;
	config.validation = true;
	config.debug = true;
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0}, config);
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	REQUIRE(instance.nativeInstance() != VK_NULL_HANDLE);
#if defined(__APPLE__)
	REQUIRE(vkGetInstanceProcAddr(instance.nativeInstance(), "vkCreateMetalSurfaceEXT") != nullptr);
	REQUIRE(vkGetInstanceProcAddr(instance.nativeInstance(), "vkCreateMacOSSurfaceMVK") == nullptr);
#endif

	const auto& gpus = instance.getAvailableGPUs();
	REQUIRE_FALSE(gpus.empty());

	bool anyGraphics = false;
	for (uint32_t i = 0; i < gpus.size(); ++i)
	{
		const auto& gpu = gpus[i];
		REQUIRE(gpu.index == i);		 // index mirrors enumeration order
		REQUIRE_FALSE(gpu.name.empty()); // neutral GPU info populated
		REQUIRE((gpu.apiVersion.major > 1 || (gpu.apiVersion.major == 1 && gpu.apiVersion.minor >= 3)));
		REQUIRE_FALSE(gpu.queueFamilies.empty());
		for (const auto& qf : gpu.queueFamilies)
			anyGraphics = anyGraphics || qf.supportsGraphics;
	}
	REQUIRE(anyGraphics);
}

TEST_CASE("Instance transfers ownership on move", "[acm][gpu]")
{
	acm::Instance a("acm-tests", acm::Version{0, 1, 0});
	if (!a.valid())
		SKIP("no Vulkan driver available");

	acm::Instance b = std::move(a);
	acm::native::InstanceHandle raw = b.nativeInstance();

	REQUIRE_FALSE(a.valid());
	REQUIRE(b.valid());
	REQUIRE(b.nativeInstance() == raw);
}
