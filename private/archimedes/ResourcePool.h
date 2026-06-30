#pragma once

#include "archimedes/acmResourceRef.h"

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace acm
{
	template <typename T>
	class ResourcePool
	{
	public:
		using RetireFn = std::function<void(T&)>;

		explicit ResourcePool(RetireFn retire)
			: m_retire(std::move(retire))
		{
		}

		template <typename Constructor>
		acm::ResourceRef<T> emplace(Constructor&& construct)
		{
			acm::ResourceSlot<T>* slot = acquireSlot();
			if (!construct(slot->resource()))
			{
				if (m_retire)
					m_retire(slot->resource());

				std::lock_guard<std::mutex> lock(m_mutex);
				m_free.push_back(slot);
				return {};
			}
			return acm::ResourceRef<T>(slot, slot->startLifetime());
		}

		void clear()
		{
			// Snapshot first: retiring an active slot recycles it through releaseSlot(),
			// which takes this pool mutex.
			std::vector<acm::ResourceSlot<T>*> slots;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				for (const auto& block : m_blocks)
				{
					for (acm::ResourceSlot<T>& slot : *block)
						slots.push_back(&slot);
				}
			}

			for (acm::ResourceSlot<T>* slot : slots)
				slot->retireActive();
		}

	private:
		acm::ResourceSlot<T>* acquireSlot()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_free.empty())
				addBlock();
			acm::ResourceSlot<T>* slot = m_free.back();
			m_free.pop_back();
			return slot;
		}

		void releaseSlot(acm::ResourceSlot<T>& slot)
		{
			if (m_retire)
				m_retire(slot.resource());
			std::lock_guard<std::mutex> lock(m_mutex);
			m_free.push_back(&slot);
		}

		void addBlock()
		{
			auto block = std::make_unique<Block>();
			for (acm::ResourceSlot<T>& slot : *block)
			{
				slot.initialize(m_nextIndex++, [this](acm::ResourceSlot<T>& releasedSlot)
								{ releaseSlot(releasedSlot); });
				m_free.push_back(&slot);
			}
			m_blocks.push_back(std::move(block));
		}

		static constexpr size_t BlockSize = 256;
		using Block = std::array<acm::ResourceSlot<T>, BlockSize>;

		RetireFn m_retire;
		std::mutex m_mutex;
		std::vector<std::unique_ptr<Block>> m_blocks;
		std::vector<acm::ResourceSlot<T>*> m_free;
		uint32_t m_nextIndex{0};
	};
} // namespace acm
