/*
 *  Created by LuckyNeko on 02/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/ResourcePool.h"

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <utility>
#include <vector>

namespace
{
	class PoolValue
	{
	public:
		PoolValue() = default;
		explicit PoolValue(uint32_t value)
			: m_value(value)
		{
		}

		bool valid() const { return true; }
		acm::Error error() const { return {}; }
		uint32_t value() const { return m_value; }

	private:
		uint32_t m_value{0};
	};

	acm::ResourceRef<PoolValue> emplaceValue(acm::ResourcePool<PoolValue>& pool, uint32_t value)
	{
		auto result = pool.emplace([value]
								   { return PoolValue(value); });
		return std::move(result.resource);
	}

	// Live slots held across a collection benchmark: a device with a populated scene.
	constexpr uint32_t LiveSlotCount = 4096;
} // namespace

TEST_CASE("resource_pool", "[bench][fast][cpu]")
{
	acm::ResourcePool<PoolValue> pool;
	acm::ResourceRef<PoolValue> resource = emplaceValue(pool, 7);
	REQUIRE(resource.valid());

	BENCHMARK("acm(ref-copy-access-10k)")
	{
		uint32_t sum = 0;
		for (uint32_t i = 0; i < 10000; ++i)
		{
			acm::ResourceRef<PoolValue> copy = resource;
			if (PoolValue* value = copy.access())
				sum += value->value();
		}
		return sum;
	};

	// The per-recorded-command cost: public wrappers call access() for every handle a
	// command names, without copying the handle.
	BENCHMARK("acm(ref-access-10k)")
	{
		uint32_t sum = 0;
		for (uint32_t i = 0; i < 10000; ++i)
		{
			if (PoolValue* value = resource.access())
				sum += value->value();
		}
		return sum;
	};

	BENCHMARK_ADVANCED("acm(pool-emplace)")(Catch::Benchmark::Chronometer meter)
	{
		std::vector<acm::ResourceRef<PoolValue>> refs(static_cast<size_t>(meter.runs()));
		meter.measure([&](int i)
					  {
			refs[static_cast<size_t>(i)] = emplaceValue(pool, static_cast<uint32_t>(i));
			return refs[static_cast<size_t>(i)].valid(); });
		for (acm::ResourceRef<PoolValue>& ref : refs)
			ref.reset();
		pool.collectGarbage();
	};

	resource.reset();
	pool.collectGarbage();
}

TEST_CASE("resource_pool_collect", "[bench][fast][cpu]")
{
	// vulkan::Device sweeps every pool once per rendered frame, so a sweep over a pool
	// full of live resources should cost what died, not what lives.
	acm::ResourcePool<PoolValue> pool;
	std::vector<acm::ResourceRef<PoolValue>> live;
	live.reserve(LiveSlotCount);
	for (uint32_t i = 0; i < LiveSlotCount; ++i)
		live.push_back(emplaceValue(pool, i));
	REQUIRE(live.back().valid());

	BENCHMARK("acm(collect-none-dead-4k-live)")
	{
		uint32_t collected = 0;
		pool.collectGarbage([&collected](PoolValue&&)
							{ ++collected; });
		return collected;
	};

	BENCHMARK_ADVANCED("acm(release-collect-1-dead-4k-live)")(Catch::Benchmark::Chronometer meter)
	{
		std::vector<acm::ResourceRef<PoolValue>> dying;
		dying.reserve(static_cast<size_t>(meter.runs()));
		for (int i = 0; i < meter.runs(); ++i)
			dying.push_back(emplaceValue(pool, static_cast<uint32_t>(i)));
		meter.measure([&](int i)
					  {
			uint32_t collected = 0;
			dying[static_cast<size_t>(i)].reset();
			pool.collectGarbage([&collected](PoolValue&&)
								{ ++collected; });
			return collected; });
	};

	for (acm::ResourceRef<PoolValue>& ref : live)
		ref.reset();
	pool.collectGarbage();
}
