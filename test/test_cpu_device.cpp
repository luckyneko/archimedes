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
#include <vector>

// The CPU device as a pathway (docs/adr/0001): device preferences rank it against hardware
// devices, and one compute shader gives the same result on either kind.

namespace
{
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
