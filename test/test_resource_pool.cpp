/*
 *  Created by LuckyNeko on 30/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/ResourcePool.h"

#include <algorithm>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <memory>
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
	// Static, so the thread lambda uses it without a capture on every compiler.
	static constexpr uint32_t Iterations = 10000;
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

	// The slot went straight back to the free list: no sweep sees it, the next emplace
	// takes it, and its generation never started a lifetime.
	uint32_t collected = 0;
	pool.collectGarbage([&collected](acmtest::SlotValue&&)
						{ ++collected; });
	REQUIRE(collected == 0);
	auto next = acmtest::emplaceValue(pool, owner, 6);
	REQUIRE(next.access()->value() == 6);
}

TEST_CASE("ResourcePool sweeps hand over only what died, in retirement order", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	std::vector<acm::ResourceRef<acmtest::SlotValue>> live;
	for (uint32_t value = 1; value <= 600; ++value)
		live.push_back(acmtest::emplaceValue(pool, owner, value));

	std::vector<uint32_t> collected;
	auto record = [&collected](acmtest::SlotValue&& value)
	{ collected.push_back(value.value()); };

	pool.collectGarbage(record);
	REQUIRE(collected.empty());

	live[599].reset();
	live[9].reset();
	acm::ResourceRef<acmtest::SlotValue> forcedCopy = live[300];
	REQUIRE(live[300].forceInvalidate());
	pool.collectGarbage(record);
	REQUIRE(collected == std::vector<uint32_t>{600, 10, 301});
	REQUIRE(owner.destroyed.load() == 3);

	collected.clear();
	pool.collectGarbage(record);
	REQUIRE(collected.empty());

	// Stale releases after the sweep retire nothing new.
	live[300].reset();
	forcedCopy.reset();
	pool.collectGarbage(record);
	REQUIRE(collected.empty());
	REQUIRE(owner.destroyed.load() == 3);
	REQUIRE(live[0].access()->value() == 1);
	REQUIRE(live[598].access()->value() == 599);
}

TEST_CASE("ResourcePool reuses a dead slot only after a sweep collected it", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	auto dead = acmtest::emplaceValue(pool, owner, 1);
	auto* deadAddress = dead.access();
	const acm::ResourceSlot<acmtest::SlotValue>::ID deadID = dead.id();
	dead.reset();

	// Enough emplaces to drain the first block and grow a second one.
	std::vector<acm::ResourceRef<acmtest::SlotValue>> later;
	for (uint32_t value = 100; value < 400; ++value)
	{
		later.push_back(acmtest::emplaceValue(pool, owner, value));
		REQUIRE(later.back().access() != deadAddress);
		REQUIRE(later.back().id().index != deadID.index);
	}
	REQUIRE(owner.destroyed.load() == 0);

	pool.collectGarbage();
	REQUIRE(owner.destroyed.load() == 1);
	auto reused = acmtest::emplaceValue(pool, owner, 2);
	REQUIRE(reused.access() == deadAddress);
	REQUIRE(reused.id().index == deadID.index);
	REQUIRE(reused.id().generation != deadID.generation);
	REQUIRE(reused.access()->value() == 2);
}

TEST_CASE("ResourcePool stale references stay harmless after their slot is reused", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	auto original = acmtest::emplaceValue(pool, owner, 1);
	acm::ResourceRef<acmtest::SlotValue> copy = original;
	const acm::ResourceSlot<acmtest::SlotValue>::ID originalID = original.id();

	REQUIRE(original.forceInvalidate());
	REQUIRE_FALSE(original.forceInvalidate());
	REQUIRE_FALSE(copy.forceInvalidate());
	REQUIRE(original.access() == nullptr);
	pool.collectGarbage();
	REQUIRE(owner.destroyed.load() == 1);

	auto reused = acmtest::emplaceValue(pool, owner, 2);
	REQUIRE(reused.id().index == originalID.index);
	REQUIRE(reused.id().generation != originalID.generation);

	// Copying, force-invalidating and releasing stale references must not touch the
	// slot's new lifetime.
	acm::ResourceRef<acmtest::SlotValue> staleCopy = copy;
	REQUIRE_FALSE(staleCopy.valid());
	REQUIRE(staleCopy.access() == nullptr);
	REQUIRE_FALSE(copy.forceInvalidate());
	copy.reset();
	original.reset();
	staleCopy.reset();
	REQUIRE(reused.valid());
	REQUIRE(reused.access()->value() == 2);
	pool.collectGarbage();
	REQUIRE(owner.destroyed.load() == 1);

	reused.reset();
	pool.collectGarbage();
	REQUIRE(owner.destroyed.load() == 2);
}

TEST_CASE("ResourcePool clear collects live and unswept dead slots exactly once", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	auto first = acmtest::emplaceValue(pool, owner, 1);
	auto second = acmtest::emplaceValue(pool, owner, 2);
	auto third = acmtest::emplaceValue(pool, owner, 3);
	second.reset();

	std::vector<uint32_t> collected;
	auto record = [&collected](acmtest::SlotValue&& value)
	{ collected.push_back(value.value()); };
	pool.clear(record);
	std::sort(collected.begin(), collected.end());
	REQUIRE(collected == std::vector<uint32_t>{1, 2, 3});
	REQUIRE(owner.destroyed.load() == 3);
	REQUIRE_FALSE(first.valid());
	REQUIRE_FALSE(third.valid());

	collected.clear();
	pool.collectGarbage(record);
	REQUIRE(collected.empty());
}

TEST_CASE("ResourcePool stays consistent under concurrent retain, release, access and collection", "[acm][unit]")
{
	// Bounded: every worker runs a fixed number of iterations and the sweepers stop once
	// all workers are done. A worker force-invalidates only resources whose copies it
	// holds itself (forced invalidation is caller-synchronized); the anchors are shared
	// by every thread and only copied, read and released.
	constexpr uint32_t WorkerCount = 4;
	constexpr uint32_t SweeperCount = 2;
	constexpr uint32_t Iterations = 10000;
	constexpr uint32_t AnchorCount = 8;
	constexpr uint32_t FirstWorkerValue = 1000;
	constexpr uint32_t ValueLimit = FirstWorkerValue + WorkerCount * Iterations;

	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool;
	std::unique_ptr<std::atomic<uint32_t>[]> collectedValues(new std::atomic<uint32_t>[ValueLimit]);
	for (uint32_t value = 0; value < ValueLimit; ++value)
		collectedValues[value].store(0, std::memory_order_relaxed);
	std::atomic<uint32_t> collected{0};
	auto sweep = [&collectedValues, &collected](acmtest::SlotValue&& value)
	{
		collectedValues[value.value()].fetch_add(1, std::memory_order_relaxed);
		collected.fetch_add(1, std::memory_order_relaxed);
	};

	std::vector<acm::ResourceRef<acmtest::SlotValue>> anchors;
	for (uint32_t anchor = 0; anchor < AnchorCount; ++anchor)
		anchors.push_back(acmtest::emplaceValue(pool, owner, anchor + 1));

	std::atomic<bool> failed{false};
	std::atomic<uint32_t> created{AnchorCount};
	std::atomic<uint32_t> workersDone{0};
	auto work = [&](uint32_t worker)
	{
		struct Owned
		{
			acm::ResourceRef<acmtest::SlotValue> ref;
			uint32_t value;
		};
		std::vector<Owned> owned;
		std::vector<acm::ResourceRef<acmtest::SlotValue>> stale;
		uint32_t random = worker * 7919u + 1u;
		auto check = [&failed](bool condition)
		{
			if (!condition)
				failed.store(true, std::memory_order_relaxed);
		};

		for (uint32_t iteration = 0; iteration < Iterations; ++iteration)
		{
			const uint32_t value = FirstWorkerValue + worker * Iterations + iteration;
			auto result = pool.emplace([&owner, value]
									   { return acmtest::SlotValue(owner, value); });
			check(result.valid());
			created.fetch_add(1, std::memory_order_relaxed);
			owned.push_back({std::move(result.resource), value});

			{
				const uint32_t anchor = (iteration + worker) % AnchorCount;
				acm::ResourceRef<acmtest::SlotValue> copy = anchors[anchor];
				check(copy.valid() && copy.access()->value() == anchor + 1);
			}

			for (const Owned& resource : owned)
			{
				const acmtest::SlotValue* payload = resource.ref.access();
				check(payload && payload->value() == resource.value);
			}
			for (const acm::ResourceRef<acmtest::SlotValue>& ref : stale)
				check(!ref.valid() && !ref.access());

			random ^= random << 13;
			random ^= random >> 17;
			random ^= random << 5;
			if (random % 4 == 0)
			{
				// Final release through the last of two copies.
				acm::ResourceRef<acmtest::SlotValue> copy = owned.back().ref;
				owned.pop_back();
				check(copy.valid());
			}
			else if (random % 4 == 1)
			{
				// Forced invalidation while a copy survives; both go stale and are
				// released later, after their slot may have been reused.
				acm::ResourceRef<acmtest::SlotValue> copy = owned.front().ref;
				check(owned.front().ref.forceInvalidate() && !copy.valid());
				stale.push_back(std::move(copy));
				stale.push_back(std::move(owned.front().ref));
				owned.erase(owned.begin());
			}
			if (owned.size() > 24)
				owned.erase(owned.begin());
			if (stale.size() > 16)
				stale.erase(stale.begin(), stale.begin() + 8);
			if (iteration % 64 == 0)
				pool.collectGarbage(sweep);
		}
		owned.clear();
		stale.clear();
		workersDone.fetch_add(1, std::memory_order_release);
	};

	std::vector<std::thread> threads;
	for (uint32_t worker = 0; worker < WorkerCount; ++worker)
		threads.emplace_back(work, worker);
	for (uint32_t sweeper = 0; sweeper < SweeperCount; ++sweeper)
	{
		threads.emplace_back([&]
							 {
			while (workersDone.load(std::memory_order_acquire) < WorkerCount)
			{
				pool.collectGarbage(sweep);
				std::this_thread::yield();
			} });
	}
	for (std::thread& thread : threads)
		thread.join();

	REQUIRE_FALSE(failed.load());
	for (uint32_t anchor = 0; anchor < AnchorCount; ++anchor)
	{
		REQUIRE(anchors[anchor].access()->value() == anchor + 1);
		anchors[anchor].reset();
	}
	pool.collectGarbage(sweep);

	const uint32_t total = created.load();
	REQUIRE(total == AnchorCount + WorkerCount * Iterations);
	REQUIRE(collected.load() == total);
	REQUIRE(owner.destroyed.load() == total);
	uint32_t collectedOnce = 0;
	bool collectedTwice = false;
	for (uint32_t value = 0; value < ValueLimit; ++value)
	{
		const uint32_t count = collectedValues[value].load(std::memory_order_relaxed);
		collectedTwice = collectedTwice || count > 1;
		collectedOnce += count;
	}
	REQUIRE_FALSE(collectedTwice);
	REQUIRE(collectedOnce == total);
}
