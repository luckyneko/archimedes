#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>

namespace acm
{
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
		using ID = acm::ResourceSlotID;
		using RetireFn = std::function<void(acm::ResourceSlot<T>&)>;

		void initialize(uint32_t index, RetireFn retire)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			assert(m_references == 0);
			m_index = index;
			m_retire = std::move(retire);
		}

		ID startLifetime()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			assert(m_retire && "ResourceSlot must be initialized before starting a lifetime");
			assert(m_references == 0);
			m_references = 1;
			return {m_index, m_generation};
		}

		T& resource() { return m_resource; }
		const T& resource() const { return m_resource; }

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
			bool retire = false;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (!matches(id))
					return;
				--m_references;
				if (m_references == 0)
				{
					advanceGeneration();
					retire = true;
				}
			}

			if (retire && m_retire)
				m_retire(*this);
		}

		bool valid(const ID& id) const
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return matches(id);
		}

		bool retire(const ID& id)
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (!matches(id))
					return false;
				m_references = 0;
				advanceGeneration();
			}

			if (m_retire)
				m_retire(*this);
			return true;
		}

		bool retireActive()
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (m_references == 0)
					return false;
				m_references = 0;
				advanceGeneration();
			}

			if (m_retire)
				m_retire(*this);
			return true;
		}

	private:
		bool matches(const ID& id) const
		{
			return id.index == m_index && id.generation == m_generation && m_references != 0;
		}

		void advanceGeneration()
		{
			assert(m_generation != UINT32_MAX && "ResourceSlot generation overflow");
			m_generation = m_generation == UINT32_MAX ? 1u : m_generation + 1u;
		}

		T m_resource;
		RetireFn m_retire;
		mutable std::mutex m_mutex;
		uint32_t m_index{0};
		uint32_t m_generation{1};
		uint32_t m_references{0};
	};
} // namespace acm
