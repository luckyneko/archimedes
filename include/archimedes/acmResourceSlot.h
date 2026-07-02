/*
 *  Created by LuckyNeko on 01/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace acm
{
	template <typename T>
	struct ResourceSlotID
	{
		static constexpr uint32_t InvalidIndex = UINT32_MAX;

		uint32_t index{InvalidIndex};
		uint32_t generation{0};
	};

	template <typename T>
	class ResourceSlot
	{
	public:
		using ID = acm::ResourceSlotID<T>;

		void initialize(uint32_t index)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			assert(m_references == 0);
			m_index = index;
			m_state = State::Free;
		}

		ID startLifetime()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			assert(m_index != ID::InvalidIndex && "ResourceSlot must be initialized before starting a lifetime");
			assert(m_state == State::Free);
			assert(m_references == 0);
			assert(m_resource.has_value());
			m_references = 1;
			m_state = State::Live;
			return {m_index, m_generation};
		}

		template <typename... Args>
		T& emplaceResource(Args&&... args)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			assert(m_state == State::Free);
			assert(!m_resource.has_value());
			return m_resource.emplace(std::forward<Args>(args)...);
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

		bool retain(const ID& id)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!matches(id))
				return false;
			assert(m_references != UINT32_MAX && "ResourceSlot reference count overflow");
			if (m_references == UINT32_MAX)
				return false;
			++m_references;
			return true;
		}

		void release(const ID& id)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!matches(id))
				return;
			--m_references;
			if (m_references == 0)
				retireLocked();
		}

		bool valid(const ID& id) const
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return matches(id);
		}

		bool forceRetire(const ID& id)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!matches(id))
				return false;
			retireLocked();
			return true;
		}

		bool forceRetire()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_state != State::Live)
				return false;
			retireLocked();
			return true;
		}

		bool collect()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_state != State::Dead)
				return false;
			m_state = State::Free;
			return true;
		}

		std::optional<T> takeResource()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_state != State::Free || !m_resource.has_value())
				return {};
			std::optional<T> resource(std::move(m_resource));
			m_resource.reset();
			return resource;
		}

		void resetResource()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_state == State::Free)
				m_resource.reset();
		}

	private:
		enum class State
		{
			Free,
			Live,
			Dead,
		};

		bool matches(const ID& id) const
		{
			return m_state == State::Live && id.index == m_index && id.generation == m_generation && m_references != 0;
		}

		void retireLocked()
		{
			m_references = 0;
			m_state = State::Dead;
			advanceGeneration();
		}

		void advanceGeneration()
		{
			assert(m_generation != UINT32_MAX && "ResourceSlot generation overflow");
			m_generation = m_generation == UINT32_MAX ? 1u : m_generation + 1u;
		}

		std::optional<T> m_resource;
		mutable std::mutex m_mutex;
		uint32_t m_index{ID::InvalidIndex};
		uint32_t m_generation{1};
		uint32_t m_references{0};
		State m_state{State::Free};
	};
} // namespace acm
