/*
 *  Created by LuckyNeko on 22/10/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

// The CPU device as a pathway (docs/adr/0001): acm::Instance loads the staged CPU driver
// directly, device preferences rank it against hardware devices, and one compute shader
// gives the same result on either kind.

namespace
{
	void setEnv(const char* name, const char* value)
	{
#ifdef _WIN32
		_putenv_s(name, value ? value : ""); // an empty value removes the variable
#else
		if (value)
			setenv(name, value, 1);
		else
			unsetenv(name);
#endif
	}

	// Restores an environment variable on scope exit, so a test leaves the process as it was.
	struct EnvGuard
	{
		explicit EnvGuard(const char* name)
			: name(name)
		{
			if (const char* value = std::getenv(name))
				saved = value;
			else
				had = false;
		}
		~EnvGuard() { setEnv(name, had ? saved.c_str() : nullptr); }

		const char* name;
		std::string saved;
		bool had{true};
	};

#ifdef ACM_EXPECT_CPU_DEVICE
	constexpr bool CpuDriverStaged = true;
#else
	constexpr bool CpuDriverStaged = false;
#endif

	size_t devicesNamed(const acm::Instance& instance, const char* name)
	{
		size_t count = 0;
		for (const acm::DeviceInfo& device : instance.devices())
			if (device.name.find(name) != std::string::npos)
				++count;
		return count;
	}

	// The first graphics option on a device of `kind`, or nullptr when there is none.
	const acm::DeviceOption* firstOfKind(const std::vector<acm::DeviceOption>& options, const acm::Instance& instance, acmtest::DeviceKind kind)
	{
		for (const acm::DeviceOption& option : options)
			if (acmtest::deviceKindOf(instance.devices()[option.deviceIndex].type) == kind)
				return &option;
		return nullptr;
	}

	// Runs computeFillSpirv (values[i] = i*2 + 1) over `count` elements and returns them.
	std::vector<uint32_t> runComputeFill(acm::Device& device, uint32_t count)
	{
		acm::Buffer storage = device.createBuffer(count * sizeof(uint32_t), acm::BufferUsage::Storage);
		REQUIRE(storage.valid());
		const std::vector<uint32_t> zeros(count, 0);
		storage.write(zeros.data(), zeros.size() * sizeof(uint32_t));

		acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
			{0, acm::DescriptorType::StorageBuffer, acm::ShaderStage::Compute},
		});
		REQUIRE(layout.valid());
		acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
		REQUIRE(descriptors.valid());
		descriptors.setBuffer(0, storage);

		acm::Shader compute = device.createShader(acmtest::computeFillSpirv());
		REQUIRE(compute.valid());
		acm::ComputePipeline pipeline = device.createComputePipeline(compute, layout);
		REQUIRE(pipeline.valid());

		REQUIRE_FALSE(device.submitSync([&](acm::CommandBuffer& cmd)
										{
											cmd.bindComputePipeline(pipeline);
											cmd.bindComputeDescriptorSet(pipeline, descriptors);
											cmd.dispatch(count / 64, 1, 1); }));

		const auto* mapped = static_cast<const uint32_t*>(storage.map());
		REQUIRE(mapped != nullptr);
		std::vector<uint32_t> values(mapped, mapped + count);
		storage.unmap();
		return values;
	}
} // namespace

TEST_CASE("DevicePreferences presets rank the CPU device", "[acm][unit]")
{
	const acm::DevicePreferences empty;
	REQUIRE(empty.types.empty());

	const acm::DevicePreferences defaults = acm::DevicePreferences::Default();
	REQUIRE(defaults.types.front() == acm::PhysicalDeviceType::DiscreteGpu);
	REQUIRE(defaults.types.back() == acm::PhysicalDeviceType::Cpu);

	const acm::DevicePreferences cpu = acm::DevicePreferences::Cpu();
	REQUIRE(cpu.types.front() == acm::PhysicalDeviceType::Cpu);
	REQUIRE(cpu.types.size() == defaults.types.size());
}

TEST_CASE("useStagedVulkanRuntime caps the CPU driver's threads without overriding the caller", "[acm][unit]")
{
	const char* const name = "SWIFTSHADER_THREAD_COUNT";
	const EnvGuard guard(name);
	setEnv(name, nullptr);

	acm::useStagedVulkanRuntime();
	REQUIRE(std::getenv(name) == nullptr); // 0 keeps the driver's default

	acm::RuntimeConfig config;
	config.cpuDeviceThreads = 1000;
	acm::useStagedVulkanRuntime(config);
	REQUIRE(std::getenv(name) != nullptr);
	REQUIRE(std::string(std::getenv(name)) == "256"); // clamped to the scheduler's limit

	config.cpuDeviceThreads = 3;
	acm::useStagedVulkanRuntime(config);
	REQUIRE(std::string(std::getenv(name)) == "256"); // a value already set wins
}

TEST_CASE("The CPU driver loads directly, without driver environment variables", "[acm][gpu]")
{
	if (!CpuDriverStaged)
		SKIP("the CPU driver is not staged");

	// Point the loader's own discovery at nothing: what an elevated Windows process gets,
	// since the loader ignores these variables there. Only the directly loaded driver remains.
	const EnvGuard driverFiles("VK_DRIVER_FILES");
	const EnvGuard icdFilenames("VK_ICD_FILENAMES");
	const EnvGuard addDriverFiles("VK_ADD_DRIVER_FILES");
	setEnv("VK_DRIVER_FILES", nullptr);
	setEnv("VK_ADD_DRIVER_FILES", nullptr);
	setEnv("VK_ICD_FILENAMES", "acm-no-such-driver.json");

	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	REQUIRE(instance.valid());
	const std::vector<acm::DeviceOption> options = instance.graphicsOptions();
	REQUIRE_FALSE(options.empty());
	REQUIRE(instance.devices()[options.front().deviceIndex].type == acm::PhysicalDeviceType::Cpu);

	// The loader cannot list a directly loaded driver's extensions before the instance
	// exists, so the surface extensions came from the driver itself.
	acm::Surface surface = instance.createHeadlessSurface(acm::Extent2D{64, 64});
	REQUIRE(surface.valid());
	REQUIRE_FALSE(instance.surfaceOptions(surface).empty());
}

TEST_CASE("InstanceConfig::cpuDriver off leaves the CPU driver unloaded", "[acm][gpu]")
{
	if (!CpuDriverStaged)
		SKIP("the CPU driver is not staged");

	// Counted against a default instance, so a system-installed SwiftShader cancels out. With
	// no other driver (a GPU-less runner) the opted-out instance may not exist at all.
	acm::Instance withDriver("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(withDriver);
	acm::InstanceConfig config;
	config.cpuDriver = false;
	acm::Instance withoutDriver("acm-tests", acm::Version{0, 1, 0}, config);
	REQUIRE(devicesNamed(withDriver, "SwiftShader") == devicesNamed(withoutDriver, "SwiftShader") + 1);
}

TEST_CASE("Device options put hardware first by default and the CPU device first on request", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);

	const std::vector<acm::DeviceOption> defaults = instance.graphicsOptions();
	const std::vector<acm::DeviceOption> cpuFirst = instance.graphicsOptions(acm::DevicePreferences::Cpu());
	REQUIRE(defaults.size() == cpuFirst.size());
	if (!firstOfKind(defaults, instance, acmtest::DeviceKind::Hardware) || !firstOfKind(defaults, instance, acmtest::DeviceKind::Cpu))
		SKIP("needs both a hardware device and the CPU device");

	REQUIRE(acmtest::deviceKindOf(instance.devices()[defaults.front().deviceIndex].type) == acmtest::DeviceKind::Hardware);
	REQUIRE(acmtest::deviceKindOf(instance.devices()[cpuFirst.front().deviceIndex].type) == acmtest::DeviceKind::Cpu);
	REQUIRE(instance.devices()[defaults.back().deviceIndex].type == acm::PhysicalDeviceType::Cpu);
}

TEST_CASE("One compute shader gives identical results on a hardware device and the CPU device", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);

	const std::vector<acm::DeviceOption> options = instance.graphicsOptions();
	const acm::DeviceOption* hardware = firstOfKind(options, instance, acmtest::DeviceKind::Hardware);
	const acm::DeviceOption* cpu = firstOfKind(options, instance, acmtest::DeviceKind::Cpu);
	if (!hardware || !cpu)
		SKIP("needs both a hardware device and the CPU device");

	// Both devices live in one process; nothing crosses between them but host memory.
	acm::Device hardwareDevice = instance.createDevice(*hardware);
	acm::Device cpuDevice = instance.createDevice(*cpu);
	REQUIRE(hardwareDevice.valid());
	REQUIRE(cpuDevice.valid());

	constexpr uint32_t kCount = 4096; // 64 workgroups of local_size_x = 64
	const std::vector<uint32_t> fromHardware = runComputeFill(hardwareDevice, kCount);
	const std::vector<uint32_t> fromCpu = runComputeFill(cpuDevice, kCount);
	REQUIRE(fromHardware == fromCpu);
	REQUIRE(fromCpu[kCount - 1] == (kCount - 1) * 2u + 1u);
}
