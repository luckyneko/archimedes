#include "archimedes/ResourcePool.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <thread>
#include <vector>

namespace acmtest
{
	struct SlotOwner
	{
		std::atomic<uint32_t> retired{0};
	};

	class SlotValue
	{
	public:
		bool create(uint32_t value)
		{
			m_value = value;
			return true;
		}

		uint32_t value() const { return m_value; }

		void retire(SlotOwner& owner)
		{
			m_value = 0;
			owner.retired.fetch_add(1, std::memory_order_relaxed);
		}

	private:
		uint32_t m_value{0};
	};
} // namespace acmtest

TEST_CASE("ResourcePool refs stay stable and reject stale generations", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool([&owner](acmtest::SlotValue& value) { value.retire(owner); });
	auto first = pool.emplace([](acmtest::SlotValue& value)
							  { return value.create(11); });
	auto* address = first.access();
	const acm::ResourceSlot<acmtest::SlotValue>::ID firstID = first.id();
	REQUIRE(first.access()->value() == 11);

	first.reset();
	REQUIRE(owner.retired.load() == 1);
	REQUIRE_FALSE(first.valid());

	auto second = pool.emplace([](acmtest::SlotValue& value)
							   { return value.create(22); });
	REQUIRE(second.access() == address);
	REQUIRE(second.id().index == firstID.index);
	REQUIRE(second.id().generation != firstID.generation);
	REQUIRE(second.access()->value() == 22);
}

TEST_CASE("ResourcePool growth does not move existing slots", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool([&owner](acmtest::SlotValue& value) { value.retire(owner); });
	auto first = pool.emplace([](acmtest::SlotValue& value)
							  { return value.create(1); });
	auto* address = first.access();
	std::vector<acm::ResourceRef<acmtest::SlotValue>> resources;
	for (uint32_t index = 0; index < 600; ++index)
		resources.push_back(pool.emplace([index](acmtest::SlotValue& value)
										 { return value.create(index); }));
	REQUIRE(first.access() == address);
	REQUIRE(first.access()->value() == 1);
}

TEST_CASE("ResourcePool refs are safe across wrapper copies", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool([&owner](acmtest::SlotValue& value) { value.retire(owner); });
	auto resource = pool.emplace([](acmtest::SlotValue& value)
								 { return value.create(7); });

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
	REQUIRE(owner.retired.load() == 0);
	resource.reset();
	REQUIRE(owner.retired.load() == 1);
}

TEST_CASE("ResourcePool forced retirement makes every old reference stale", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool([&owner](acmtest::SlotValue& value) { value.retire(owner); });
	auto resource = pool.emplace([](acmtest::SlotValue& value)
								 { return value.create(9); });
	acm::ResourceRef<acmtest::SlotValue> copy = resource;
	REQUIRE(copy.valid());
	REQUIRE(resource.forceInvalidate());
	REQUIRE_FALSE(resource.valid());
	REQUIRE_FALSE(copy.valid());
	REQUIRE(owner.retired.load() == 1);

	auto reused = pool.emplace([](acmtest::SlotValue& value)
							   { return value.create(10); });
	REQUIRE(reused.id().index == resource.id().index);
	REQUIRE(reused.access()->value() == 10);
}

TEST_CASE("ResourcePool clear retires each active slot exactly once", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::ResourcePool<acmtest::SlotValue> pool([&owner](acmtest::SlotValue& value) { value.retire(owner); });
	auto first = pool.emplace([](acmtest::SlotValue& value)
							  { return value.create(1); });
	auto second = pool.emplace([](acmtest::SlotValue& value)
							   { return value.create(2); });
	acm::ResourceRef<acmtest::SlotValue> firstCopy = first;

	pool.clear();
	REQUIRE(owner.retired.load(std::memory_order_relaxed) == 2);
	REQUIRE_FALSE(first.valid());
	REQUIRE_FALSE(firstCopy.valid());
	REQUIRE_FALSE(second.valid());

	first.reset();
	firstCopy.reset();
	second.reset();
	REQUIRE(owner.retired.load(std::memory_order_relaxed) == 2);
}
