/*
 *  Created by LuckyNeko on 01/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/DeferredDestroyQueue.h"

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace acmtest
{
	class DeferredResource
	{
	public:
		explicit DeferredResource(std::shared_ptr<bool> destroyed)
			: m_destroyed(std::move(destroyed))
		{
		}

		~DeferredResource()
		{
			if (m_destroyed)
				*m_destroyed = true;
		}

		DeferredResource(const DeferredResource&) = delete;
		DeferredResource& operator=(const DeferredResource&) = delete;
		DeferredResource(DeferredResource&&) noexcept = default;
		DeferredResource& operator=(DeferredResource&&) noexcept = default;

	private:
		std::shared_ptr<bool> m_destroyed;
	};

	// Appends its id to a shared log when destroyed (moved-from instances stay silent).
	class OrderedResource
	{
	public:
		OrderedResource(std::vector<uint32_t>& log, uint32_t id)
			: m_log(&log)
			, m_id(id)
		{
		}

		~OrderedResource()
		{
			if (m_log)
				m_log->push_back(m_id);
		}

		OrderedResource(const OrderedResource&) = delete;
		OrderedResource& operator=(const OrderedResource&) = delete;
		OrderedResource(OrderedResource&& other) noexcept
			: m_log(std::exchange(other.m_log, nullptr))
			, m_id(other.m_id)
		{
		}
		OrderedResource& operator=(OrderedResource&&) = delete;

	private:
		std::vector<uint32_t>* m_log{nullptr};
		uint32_t m_id{0};
	};

	// Enqueues another resource from its destructor, as a payload releasing a
	// dependency may; that only works if collect() destroys outside its lock.
	class ReenteringResource
	{
	public:
		ReenteringResource(acm::DeferredDestroyQueue& queue, uint64_t serial, std::shared_ptr<bool> innerDestroyed)
			: m_queue(&queue)
			, m_serial(serial)
			, m_innerDestroyed(std::move(innerDestroyed))
		{
		}

		~ReenteringResource()
		{
			if (m_queue)
				m_queue->enqueue(m_serial, DeferredResource(std::move(m_innerDestroyed)));
		}

		ReenteringResource(const ReenteringResource&) = delete;
		ReenteringResource& operator=(const ReenteringResource&) = delete;
		ReenteringResource(ReenteringResource&& other) noexcept
			: m_queue(std::exchange(other.m_queue, nullptr))
			, m_serial(other.m_serial)
			, m_innerDestroyed(std::move(other.m_innerDestroyed))
		{
		}
		ReenteringResource& operator=(ReenteringResource&&) = delete;

	private:
		acm::DeferredDestroyQueue* m_queue{nullptr};
		uint64_t m_serial{0};
		std::shared_ptr<bool> m_innerDestroyed;
	};
} // namespace acmtest

TEST_CASE("DeferredDestroyQueue stores move-only resources until their submission completes", "[acm][unit]")
{
	acm::DeferredDestroyQueue queue;
	auto destroyed = std::make_shared<bool>(false);
	auto resource = acmtest::DeferredResource(destroyed);

	queue.enqueue(3, std::move(resource));

	REQUIRE_FALSE(queue.empty());
	queue.collect(2);
	REQUIRE_FALSE(*destroyed);
	REQUIRE_FALSE(queue.empty());

	queue.collect(3);
	REQUIRE(*destroyed);
	REQUIRE(queue.empty());
}

TEST_CASE("DeferredDestroyQueue destroys ready entries in enqueue order and keeps the rest", "[acm][unit]")
{
	std::vector<uint32_t> log;
	acm::DeferredDestroyQueue queue;
	queue.enqueue(1, acmtest::OrderedResource(log, 1));
	queue.enqueue(3, acmtest::OrderedResource(log, 2));
	queue.enqueue(2, acmtest::OrderedResource(log, 3));
	queue.enqueue(1, acmtest::OrderedResource(log, 4));
	queue.enqueue(4, acmtest::OrderedResource(log, 5));
	queue.enqueue(2, acmtest::OrderedResource(log, 6));
	REQUIRE(log.empty());

	queue.collect(0);
	REQUIRE(log.empty());

	queue.collect(2);
	REQUIRE(log == std::vector<uint32_t>{1, 3, 4, 6});

	queue.enqueue(3, acmtest::OrderedResource(log, 7));
	queue.collect(3);
	REQUIRE(log == std::vector<uint32_t>{1, 3, 4, 6, 2, 7});
	REQUIRE_FALSE(queue.empty());

	queue.collect(3);
	REQUIRE(log.size() == 6);

	queue.collect(UINT64_MAX);
	REQUIRE(log == std::vector<uint32_t>{1, 3, 4, 6, 2, 7, 5});
	REQUIRE(queue.empty());
}

TEST_CASE("DeferredDestroyQueue collects a long interleaved queue in one pass", "[acm][unit]")
{
	constexpr uint32_t EntryCount = 20000;
	std::vector<uint32_t> log;
	log.reserve(EntryCount);
	acm::DeferredDestroyQueue queue;
	for (uint32_t id = 0; id < EntryCount; ++id)
		queue.enqueue(id % 2 == 0 ? 1 : 2, acmtest::OrderedResource(log, id));

	queue.collect(1);
	REQUIRE(log.size() == EntryCount / 2);
	bool evenInOrder = true;
	for (uint32_t index = 0; index < log.size(); ++index)
		evenInOrder = evenInOrder && log[index] == index * 2;
	REQUIRE(evenInOrder);

	queue.collect(2);
	REQUIRE(log.size() == EntryCount);
	bool oddInOrder = true;
	for (uint32_t index = EntryCount / 2; index < log.size(); ++index)
		oddInOrder = oddInOrder && log[index] == (index - EntryCount / 2) * 2 + 1;
	REQUIRE(oddInOrder);
	REQUIRE(queue.empty());
}

TEST_CASE("DeferredDestroyQueue runs destructors after releasing its lock", "[acm][unit]")
{
	acm::DeferredDestroyQueue queue;
	auto innerDestroyed = std::make_shared<bool>(false);
	queue.enqueue(1, acmtest::ReenteringResource(queue, 1, innerDestroyed));

	// The outer destructor enqueues during this collect; the new entry is ready by
	// serial but waits for the next collect.
	queue.collect(1);
	REQUIRE_FALSE(*innerDestroyed);
	REQUIRE_FALSE(queue.empty());

	queue.collect(1);
	REQUIRE(*innerDestroyed);
	REQUIRE(queue.empty());
}
