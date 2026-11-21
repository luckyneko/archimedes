/*
 *  Created by LuckyNeko on 30/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmResourceRef.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace acm
{
	// Block-backed stable-slot pool. Slots never move, so ResourceRef can retain a
	// typed slot pointer plus generation until the owning root retires it.
	//
	// A slot that retires (final release or forced invalidation) pushes itself onto the
	// pool's lock-free retired list, so a sweep walks only what died since the last one:
	// one atomic load and nothing else when nothing did. Swept slots return to an
	// intrusive free list under the pool mutex. Neither list allocates. The pool mutex
	// never nests a slot operation that could block, and retiring never takes it, so
	// ResourceRef reaches its slot without knowing the pool.
	template <typename T>
	class ResourcePool
	{
		using Slot = acm::ResourceSlot<T>;

	public:
		// Result
		struct EmplaceResult
		{
			acm::ResourceRef<T> resource;
			acm::Error error;

			bool valid() const { return resource.valid(); }
		};

		ResourcePool() = default;
		ResourcePool(const ResourcePool&) = delete;
		ResourcePool& operator=(const ResourcePool&) = delete;

		// Allocation
		template <typename Constructor>
		EmplaceResult emplace(Constructor&& construct)
		{
			Slot* slot = acquireSlot();
			T& resource = slot->emplaceResource(construct());
			if (!resource.valid())
			{
				acm::Error error = resource.error();
				slot->resetResource();
				recycle(slot, slot);
				return {{}, std::move(error)};
			}
			return {acm::ResourceRef<T>(slot, slot->startLifetime()), {}};
		}

		// Collection
		//
		// clear() force-retires every live slot (O(slots); device/instance teardown) and
		// sweeps them, handing payloads to `collect` in slot order.
		template <typename Collect>
		void clear(Collect&& collect)
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				for (const auto& block : m_blocks)
				{
					for (Slot& slot : *block)
						slot.forceRetire();
				}
			}
			collectGarbage(collect);
		}

		void clear()
		{
			clear([](T&&) {});
		}

		// Moves each payload retired since the last sweep out of its slot, hands it to
		// `collect` in retirement order, then returns the emptied slots to the free list.
		// Concurrent sweeps each claim a disjoint set of retired slots.
		template <typename Collect>
		void collectGarbage(Collect&& collect)
		{
			if (m_retired.load(std::memory_order_relaxed) == nullptr)
				return;
			Slot* first = reverse(m_retired.exchange(nullptr, std::memory_order_acquire));
			Slot* last = nullptr;
			try
			{
				for (Slot* slot = first; slot; slot = slot->next())
				{
					last = slot;
					if (std::optional<T> resource = slot->takeResource())
						collect(std::move(*resource));
				}
			}
			catch (...)
			{
				// `last` is empty now; keep what was swept and republish the rest.
				Slot* unswept = last->next();
				recycle(first, last);
				if (unswept)
					republish(unswept);
				throw;
			}
			if (first)
				recycle(first, last);
		}

		void collectGarbage()
		{
			collectGarbage([](T&&) {});
		}

	private:
		// Internals
		Slot* acquireSlot()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_free)
				addBlock();
			Slot* slot = m_free;
			m_free = slot->next();
			slot->setNext(nullptr);
			return slot;
		}

		// Returns the linked run first..last to the free list.
		void recycle(Slot* first, Slot* last)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			last->setNext(m_free);
			m_free = first;
		}

		// Pushes a chain back onto the retired list after a sweep failed part-way.
		void republish(Slot* first)
		{
			Slot* last = first;
			while (last->next())
				last = last->next();
			Slot* head = m_retired.load(std::memory_order_relaxed);
			do
			{
				last->setNext(head);
			} while (!m_retired.compare_exchange_weak(head, first, std::memory_order_release, std::memory_order_relaxed));
		}

		// The retired list is a LIFO stack; sweeping oldest-first keeps payload hand-off
		// in retirement order.
		static Slot* reverse(Slot* slot)
		{
			Slot* reversed = nullptr;
			while (slot)
			{
				Slot* next = slot->next();
				slot->setNext(reversed);
				reversed = slot;
				slot = next;
			}
			return reversed;
		}

		void addBlock()
		{
			auto block = std::make_unique<Block>();
			for (Slot& slot : *block)
			{
				slot.initialize(m_nextIndex++, m_retired);
				slot.setNext(m_free);
				m_free = &slot;
			}
			m_blocks.push_back(std::move(block));
		}

		static constexpr size_t BlockSize = 256;
		using Block = std::array<Slot, BlockSize>;

		std::atomic<Slot*> m_retired{nullptr};
		std::mutex m_mutex;
		std::vector<std::unique_ptr<Block>> m_blocks;
		Slot* m_free{nullptr};
		uint32_t m_nextIndex{0};
	};
} // namespace acm
