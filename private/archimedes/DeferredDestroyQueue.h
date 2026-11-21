/*
 *  Created by LuckyNeko on 01/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace acm
{
	// Thread-safe type-erased queue for backend resources whose destructors must run
	// only after a submitted queue serial has completed.
	class DeferredDestroyQueue
	{
	public:
		// Enqueue
		template <typename T>
		void enqueue(uint64_t submissionSerial, T&& resource)
		{
			using EntryType = Entry<std::decay_t<T>>;
			auto entry = std::make_unique<EntryType>(std::forward<T>(resource));
			std::lock_guard<std::mutex> lock(m_mutex);
			m_pending.push_back({submissionSerial, std::move(entry)});
		}

		// Collection
		//
		// Destroys every entry whose serial is <= completedSerial, in enqueue order, after
		// the queue mutex is released (a destructor may enqueue more; those wait for a
		// later collect). One linear pass keeps the rest in enqueue order.
		void collect(uint64_t completedSerial)
		{
			std::vector<std::unique_ptr<EntryBase>> ready;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				auto kept = m_pending.begin();
				for (auto pending = m_pending.begin(); pending != m_pending.end(); ++pending)
				{
					if (pending->submissionSerial <= completedSerial)
						ready.push_back(std::move(pending->entry));
					else
					{
						if (kept != pending)
							*kept = std::move(*pending);
						++kept;
					}
				}
				m_pending.erase(kept, m_pending.end());
			}
			for (std::unique_ptr<EntryBase>& entry : ready)
				entry.reset();
		}

		// State
		bool empty() const
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_pending.empty();
		}

	private:
		// Internals
		struct EntryBase
		{
			virtual ~EntryBase() = default;
		};

		template <typename T>
		struct Entry : EntryBase
		{
			explicit Entry(T&& resource)
				: resource(std::move(resource))
			{
			}

			T resource;
		};

		struct Pending
		{
			uint64_t submissionSerial{0};
			std::unique_ptr<EntryBase> entry;
		};

		mutable std::mutex m_mutex;
		std::vector<Pending> m_pending;
	};
} // namespace acm
