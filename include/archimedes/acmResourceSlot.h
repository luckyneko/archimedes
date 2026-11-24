/*
 *  Created by LuckyNeko on 01/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <optional>
#include <utility>

namespace acm
{
	template <typename T>
	class ResourcePool;

	template <typename T>
	struct ResourceSlotID
	{
		static constexpr uint32_t InvalidIndex = UINT32_MAX;

		uint32_t index{InvalidIndex};
		uint32_t generation{0};
	};

	// One stable ResourcePool slot: an optional backend payload plus its lifetime state.
	//
	// The lifetime state is a single 64-bit atomic word, generation in the high half and
	// reference count in the low half. References > 0 means live; a ResourceRef matches
	// when its ID carries the word's generation. Retirement (the final release, or a
	// forced invalidation) is one compare-exchange from (g, n > 0) to (g + 1, 0), so
	// exactly one thread retires each lifetime; that thread then publishes the slot to
	// its pool's retired list as its last touch of the slot. Between retirement and the
	// next startLifetime() the slot belongs to whichever pool operation holds it (the
	// retired list, a sweep, the free list, an emplace), and that holder alone moves the
	// payload in or out.
	//
	// Reads (valid / access) are one acquire load and a compare: no lock, no write.
	//
	// Memory ordering: startLifetime() publishes the payload with a release store. Every
	// retain / release / retirement of a lifetime is an acq_rel read-modify-write on that
	// word, so the retiring RMW is ordered after every earlier holder's uses; the
	// retired-list push is a release and a sweep claims the list with an acquire, so the
	// payload leaves the slot only after every use through a reference that was live.
	//
	// Stale references only ever compare-exchange a word whose generation matches theirs
	// and whose count is non-zero, so they never write a retired or reused slot. The
	// generation advances on every retirement and a slot is reused only after a sweep,
	// so a stale reference could match again only after 2^32 lifetimes of the same slot
	// (asserted; the wrap skips 0, which a default ID carries).
	//
	// Forced invalidation and mutation of the same wrapper object stay caller-synchronized:
	// access() returns the payload after validating, and a concurrent forced invalidation
	// plus sweep could move the payload out from under that pointer.
	template <typename T>
	class ResourceSlot
	{
	public:
		using ID = acm::ResourceSlotID<T>;

		ResourceSlot() = default;
		ResourceSlot(const ResourceSlot&) = delete;
		ResourceSlot& operator=(const ResourceSlot&) = delete;

		// Reference operations: any thread, through a ResourceRef
		bool retain(const ID& id)
		{
			uint64_t state = m_state.load(std::memory_order_relaxed);
			do
			{
				if (!matches(state, id))
					return false;
				assert(referencesOf(state) != UINT32_MAX && "ResourceSlot reference count overflow");
				if (referencesOf(state) == UINT32_MAX)
					return false;
			} while (!m_state.compare_exchange_weak(state, state + 1, std::memory_order_acq_rel, std::memory_order_relaxed));
			return true;
		}

		void release(const ID& id)
		{
			uint64_t state = m_state.load(std::memory_order_relaxed);
			uint64_t next = 0;
			do
			{
				if (!matches(state, id))
					return;
				next = referencesOf(state) == 1 ? retiredState(state) : state - 1;
			} while (!m_state.compare_exchange_weak(state, next, std::memory_order_acq_rel, std::memory_order_relaxed));
			if (referencesOf(next) == 0)
				publishRetired();
		}

		bool valid(const ID& id) const
		{
			return matches(m_state.load(std::memory_order_acquire), id);
		}

		bool forceRetire(const ID& id)
		{
			uint64_t state = m_state.load(std::memory_order_relaxed);
			do
			{
				if (!matches(state, id))
					return false;
			} while (!m_state.compare_exchange_weak(state, retiredState(state), std::memory_order_acq_rel, std::memory_order_relaxed));
			publishRetired();
			return true;
		}

		T& resource()
		{
			assert(m_resource.has_value());
			return *m_resource;
		}

		const T& resource() const
		{
			assert(m_resource.has_value());
			return *m_resource;
		}

	private:
		friend class acm::ResourcePool<T>;

		// Pool operations: only the pool operation that currently holds the slot
		void initialize(uint32_t index, std::atomic<ResourceSlot*>& retired)
		{
			assert(referencesOf(m_state.load(std::memory_order_relaxed)) == 0);
			m_index = index;
			m_retired = &retired;
		}

		template <typename... Args>
		T& emplaceResource(Args&&... args)
		{
			assert(referencesOf(m_state.load(std::memory_order_relaxed)) == 0);
			assert(!m_resource.has_value());
			return m_resource.emplace(std::forward<Args>(args)...);
		}

		ID startLifetime()
		{
			// No other thread writes a retired word (see above), so a plain store is safe;
			// release publishes the payload to every thread that later validates the ID.
			const uint64_t state = m_state.load(std::memory_order_relaxed);
			assert(m_index != ID::InvalidIndex && "ResourceSlot must be initialized before starting a lifetime");
			assert(referencesOf(state) == 0);
			assert(m_resource.has_value());
			m_state.store(state + 1, std::memory_order_release);
			return {m_index, generationOf(state)};
		}

		// Retires whatever lifetime is live (pool teardown).
		bool forceRetire()
		{
			uint64_t state = m_state.load(std::memory_order_relaxed);
			do
			{
				if (referencesOf(state) == 0)
					return false;
			} while (!m_state.compare_exchange_weak(state, retiredState(state), std::memory_order_acq_rel, std::memory_order_relaxed));
			publishRetired();
			return true;
		}

		std::optional<T> takeResource()
		{
			assert(referencesOf(m_state.load(std::memory_order_relaxed)) == 0);
			std::optional<T> resource(std::move(m_resource));
			m_resource.reset();
			return resource;
		}

		void resetResource()
		{
			assert(referencesOf(m_state.load(std::memory_order_relaxed)) == 0);
			m_resource.reset();
		}

		// Intrusive link for the pool's retired list or free list; a slot is on at most one.
		ResourceSlot* next() const { return m_next; }
		void setNext(ResourceSlot* next) { m_next = next; }

		// Internals
		static constexpr uint32_t generationOf(uint64_t state) { return static_cast<uint32_t>(state >> 32); }
		static constexpr uint32_t referencesOf(uint64_t state) { return static_cast<uint32_t>(state); }

		static uint64_t retiredState(uint64_t state)
		{
			const uint32_t generation = generationOf(state);
			assert(generation != UINT32_MAX && "ResourceSlot generation overflow");
			const uint32_t next = generation == UINT32_MAX ? 1u : generation + 1u;
			return static_cast<uint64_t>(next) << 32;
		}

		bool matches(uint64_t state, const ID& id) const
		{
			return generationOf(state) == id.generation && referencesOf(state) != 0 && id.index == m_index;
		}

		// Lock-free push onto the pool's retired list. A sweep may recycle the slot as
		// soon as the exchange succeeds, so nothing touches the slot after it.
		void publishRetired()
		{
			std::atomic<ResourceSlot*>& retired = *m_retired;
			ResourceSlot* head = retired.load(std::memory_order_relaxed);
			do
			{
				m_next = head;
			} while (!retired.compare_exchange_weak(head, this, std::memory_order_release, std::memory_order_relaxed));
		}

		static_assert(std::atomic<uint64_t>::is_always_lock_free, "ResourceSlot needs a lock-free 64-bit atomic");

		std::atomic<uint64_t> m_state{static_cast<uint64_t>(1) << 32};
		uint32_t m_index{ID::InvalidIndex};
		ResourceSlot* m_next{nullptr};
		std::atomic<ResourceSlot*>* m_retired{nullptr};
		std::optional<T> m_resource;
	};
} // namespace acm
