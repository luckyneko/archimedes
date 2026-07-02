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
