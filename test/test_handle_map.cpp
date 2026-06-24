#include "archimedes/HandleMap.h"

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

	class SlotValue : public acm::ResourceSlot<SlotValue, SlotOwner>
	{
	public:
		static auto insert(acm::HandleMap<SlotValue, SlotOwner>& map, uint32_t value)
		{
			return map.emplace([value](SlotValue& slot)
							   { return slot.create(value); });
		}

		bool create(uint32_t value)
		{
			m_value = value;
			return true;
		}
		uint32_t value(const acm::Handle& handle) const { return accessible(handle) ? m_value : 0; }

		void retire(SlotOwner& owner)
		{
			m_value = 0;
			owner.retired.fetch_add(1, std::memory_order_relaxed);
		}

	private:
		uint32_t m_value{0};
	};
} // namespace acmtest

TEST_CASE("HandleMap slots stay stable and reject stale generations", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::HandleMap<acmtest::SlotValue, acmtest::SlotOwner> map(owner);
	auto first = acmtest::SlotValue::insert(map, 11);
	auto* address = first.resource;
	REQUIRE(first.resource->value(first.handle) == 11);

	first.resource->release(first.handle);
	REQUIRE(owner.retired.load() == 1);
	REQUIRE(first.resource->value(first.handle) == 0);

	auto second = acmtest::SlotValue::insert(map, 22);
	REQUIRE(second.resource == address);
	REQUIRE(second.handle.index() == first.handle.index());
	REQUIRE(second.handle.generation() != first.handle.generation());
	REQUIRE(second.resource->value(second.handle) == 22);
	first.resource->release(first.handle);
	REQUIRE(second.resource->referenceCount() == 1);
}

TEST_CASE("HandleMap growth does not move existing slots", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::HandleMap<acmtest::SlotValue, acmtest::SlotOwner> map(owner);
	auto first = acmtest::SlotValue::insert(map, 1);
	auto* address = first.resource;
	std::vector<decltype(first)> resources;
	for (uint32_t index = 0; index < 600; ++index)
		resources.push_back(acmtest::SlotValue::insert(map, index));
	REQUIRE(first.resource == address);
	REQUIRE(first.resource->value(first.handle) == 1);
	first.resource->release(first.handle);
	for (auto& resource : resources)
		resource.resource->release(resource.handle);
}

TEST_CASE("HandleMap retain and release are safe across wrapper copies", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::HandleMap<acmtest::SlotValue, acmtest::SlotOwner> map(owner);
	auto resource = acmtest::SlotValue::insert(map, 7);

	constexpr uint32_t ThreadCount = 8;
	constexpr uint32_t Iterations = 10000;
	std::atomic<bool> failed{false};
	std::vector<std::thread> threads;
	for (uint32_t thread = 0; thread < ThreadCount; ++thread)
		threads.emplace_back([slot = resource.resource, handle = resource.handle, &failed]
							 {
							 for (uint32_t iteration = 0; iteration < Iterations; ++iteration)
							 {
								 if (!slot->retain(handle))
								 {
									 failed.store(true, std::memory_order_relaxed);
									 return;
								 }
								 if (slot->value(handle) != 7)
									 failed.store(true, std::memory_order_relaxed);
								 slot->release(handle);
							 } });
	for (std::thread& thread : threads)
		thread.join();

	REQUIRE_FALSE(failed.load(std::memory_order_relaxed));
	REQUIRE(resource.resource->referenceCount() == 1);
	resource.resource->release(resource.handle);
	REQUIRE(owner.retired.load() == 1);
}

TEST_CASE("HandleMap invalidation makes every old reference stale", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::HandleMap<acmtest::SlotValue, acmtest::SlotOwner> map(owner);
	auto resource = acmtest::SlotValue::insert(map, 9);
	REQUIRE(resource.resource->retain(resource.handle));
	REQUIRE(resource.resource->forceInvalidate(resource.handle));
	REQUIRE(resource.resource->value(resource.handle) == 0);
	resource.resource->release(resource.handle);
	REQUIRE(owner.retired.load() == 1);

	auto reused = acmtest::SlotValue::insert(map, 10);
	REQUIRE(reused.resource == resource.resource);
	resource.resource->release(resource.handle);
	REQUIRE(reused.resource->referenceCount() == 1);
	reused.resource->release(reused.handle);
}

TEST_CASE("HandleMap clear retires each active slot exactly once", "[acm][unit]")
{
	acmtest::SlotOwner owner;
	acm::HandleMap<acmtest::SlotValue, acmtest::SlotOwner> map(owner);
	auto first = acmtest::SlotValue::insert(map, 1);
	auto second = acmtest::SlotValue::insert(map, 2);
	REQUIRE(first.resource->retain(first.handle));

	map.clear();
	REQUIRE(owner.retired.load(std::memory_order_relaxed) == 2);
	REQUIRE(first.resource->value(first.handle) == 0);
	REQUIRE(second.resource->value(second.handle) == 0);

	first.resource->release(first.handle);
	first.resource->release(first.handle);
	second.resource->release(second.handle);
	REQUIRE(owner.retired.load(std::memory_order_relaxed) == 2);
}
