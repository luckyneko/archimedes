/*
 *  Created by LuckyNeko on 30/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/ResourcePool.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <thread>
#include <utility>
#include <vector>

namespace acmtest
{
	struct SlotOwner
	{
		std::atomic<uint32_t> destroyed{0};
	};

	class SlotValue
	{
	public:
		SlotValue() = default;
		SlotValue(SlotOwner& owner, uint32_t value, bool valid = true, acm::Error error = {})
			: m_owner(&owner)
			, m_value(value)
			, m_valid(valid)
			, m_error(std::move(error))
		{
		}

		~SlotValue()
		{
			release();
		}

		SlotValue(const SlotValue&) = delete;
		SlotValue& operator=(const SlotValue&) = delete;
		SlotValue(SlotValue&& other) noexcept
		{
			*this = std::move(other);
		}

		SlotValue& operator=(SlotValue&& other) noexcept
		{
			if (this == &other)
				return *this;
			release();
			m_owner = std::exchange(other.m_owner, nullptr);
			m_value = std::exchange(other.m_value, 0);
			m_valid = std::exchange(other.m_valid, false);
			m_error = std::move(other.m_error);
			return *this;
		}

		bool valid() const { return m_valid; }
		acm::Error error() const { return m_error; }
		uint32_t value() const { return m_value; }

	private:
		void release()
		{
			SlotOwner* owner = std::exchange(m_owner, nullptr);
			const uint32_t value = std::exchange(m_value, 0);
			m_valid = false;
			if (owner && value != 0)
				owner->destroyed.fetch_add(1, std::memory_order_relaxed);
		}

		SlotOwner* m_owner{nullptr};
		uint32_t m_value{0};
		bool m_valid{false};
		acm::Error m_error;
	};

	acm::ResourceRef<SlotValue> emplaceValue(acm::ResourcePool<SlotValue>& pool, SlotOwner& owner, uint32_t value)
	{
		auto result = pool.emplace([&owner, value]
								   { return SlotValue(owner, value); });
		REQUIRE(result.valid());
		return std::move(result.resource);
	}
} // namespace acmtest

TEST_CASE("ResourcePool refs stay stable and reject stale generations", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	auto first = acmtest::emplaceValue(pool, owner, 11);
	auto* address = first.access();
	const acm::ResourceSlot<acmtest::SlotValue>::ID firstID = first.id();
	REQUIRE(first.access()->value() == 11);

	first.reset();
	REQUIRE(owner.destroyed.load() == 0);
	REQUIRE_FALSE(first.valid());

	auto second = acmtest::emplaceValue(pool, owner, 22);
	REQUIRE(second.access() != address);
	REQUIRE(second.id().index != firstID.index);
	pool.collectGarbage();
	REQUIRE(owner.destroyed.load() == 1);

	auto third = acmtest::emplaceValue(pool, owner, 33);
	REQUIRE(third.access() == address);
	REQUIRE(third.id().index == firstID.index);
	REQUIRE(third.id().generation != firstID.generation);
	REQUIRE(second.access()->value() == 22);
	REQUIRE(third.access()->value() == 33);
}

TEST_CASE("ResourcePool growth does not move existing slots", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	auto first = acmtest::emplaceValue(pool, owner, 1);
	auto* address = first.access();
	std::vector<acm::ResourceRef<acmtest::SlotValue>> resources;
	for (uint32_t index = 0; index < 600; ++index)
		resources.push_back(acmtest::emplaceValue(pool, owner, index));
	REQUIRE(first.access() == address);
	REQUIRE(first.access()->value() == 1);
}

TEST_CASE("ResourcePool refs are safe across wrapper copies", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	auto resource = acmtest::emplaceValue(pool, owner, 7);

	constexpr uint32_t ThreadCount = 8;
	constexpr uint32_t Iterations = 10000;
	std::atomic<bool> failed{false};
	std::vector<std::thread> threads;
	for (uint32_t thread = 0; thread < ThreadCount; ++thread)
		threads.emplace_back([resource, &failed]
							 {
							 for (uint32_t iteration = 0; iteration < Iterations; ++iteration)
							 {
								 acm::ResourceRef<acmtest::SlotValue> copy = resource;
								 if (!copy.valid() || copy.access()->value() != 7)
								 {
									 failed.store(true, std::memory_order_relaxed);
									 return;
								 }
							 } });
	for (std::thread& thread : threads)
		thread.join();

	REQUIRE_FALSE(failed.load(std::memory_order_relaxed));
	REQUIRE(owner.destroyed.load() == 0);
	resource.reset();
	REQUIRE(owner.destroyed.load() == 0);
	pool.collectGarbage();
	REQUIRE(owner.destroyed.load() == 1);
}

TEST_CASE("ResourcePool forced invalidation makes every old reference stale", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	auto resource = acmtest::emplaceValue(pool, owner, 9);
	acm::ResourceRef<acmtest::SlotValue> copy = resource;
	REQUIRE(copy.valid());
	REQUIRE(resource.forceInvalidate());
	REQUIRE_FALSE(resource.valid());
	REQUIRE_FALSE(copy.valid());
	REQUIRE(owner.destroyed.load() == 0);

	pool.collectGarbage();
	REQUIRE(owner.destroyed.load() == 1);
	auto reused = acmtest::emplaceValue(pool, owner, 10);
	REQUIRE(reused.id().index == resource.id().index);
	REQUIRE(reused.access()->value() == 10);
}

TEST_CASE("ResourcePool clear destroys each active slot exactly once", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	auto first = acmtest::emplaceValue(pool, owner, 1);
	auto second = acmtest::emplaceValue(pool, owner, 2);
	acm::ResourceRef<acmtest::SlotValue> firstCopy = first;

	pool.clear();
	REQUIRE(owner.destroyed.load(std::memory_order_relaxed) == 2);
	REQUIRE_FALSE(first.valid());
	REQUIRE_FALSE(firstCopy.valid());
	REQUIRE_FALSE(second.valid());

	first.reset();
	firstCopy.reset();
	second.reset();
	REQUIRE(owner.destroyed.load(std::memory_order_relaxed) == 2);
}

TEST_CASE("ResourcePool destroys failed construction immediately", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;

	auto result = pool.emplace([&owner]
							   { return acmtest::SlotValue(owner, 5, false, acm::Error("construction failed")); });

	REQUIRE_FALSE(result.valid());
	REQUIRE(result.error.message() == std::string("construction failed"));
	REQUIRE(owner.destroyed.load(std::memory_order_relaxed) == 1);
}
