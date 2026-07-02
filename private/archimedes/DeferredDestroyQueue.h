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
	class DeferredDestroyQueue
	{
	public:
		template <typename T>
		void enqueue(uint64_t submissionSerial, T&& resource)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			using EntryType = Entry<std::decay_t<T>>;
			m_pending.push_back({submissionSerial, std::make_unique<EntryType>(std::forward<T>(resource))});
		}

		void collect(uint64_t completedSerial)
		{
			std::vector<std::unique_ptr<EntryBase>> ready;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				auto pending = m_pending.begin();
				while (pending != m_pending.end())
				{
					if (pending->submissionSerial <= completedSerial)
					{
						ready.push_back(std::move(pending->entry));
						pending = m_pending.erase(pending);
					}
					else
					{
						++pending;
					}
				}
			}
			ready.clear();
		}

		bool empty() const
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_pending.empty();
		}

	private:
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
