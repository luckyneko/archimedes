#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmResourceRef.h"

#include <array>
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
		struct EmplaceResult
		{
			acm::ResourceRef<T> resource;
			acm::Error error;

			bool valid() const { return resource.valid(); }
		};

		ResourcePool() = default;

		template <typename Constructor>
		EmplaceResult emplace(Constructor&& construct)
		{
			acm::ResourceSlot<T>* slot = acquireSlot();
			T& resource = slot->emplaceResource(construct());
			if (!resource.valid())
			{
				acm::Error error = resource.error();
				slot->resetResource();
				std::lock_guard<std::mutex> lock(m_mutex);
				m_free.push_back(slot);
				return {{}, std::move(error)};
			}
			return {acm::ResourceRef<T>(slot, slot->startLifetime()), {}};
		}

		template <typename Collect>
		void clear(Collect&& collect)
		{
			std::vector<acm::ResourceSlot<T>*> slots = snapshotSlots();
			for (acm::ResourceSlot<T>* slot : slots)
				slot->forceRetire();
			collectGarbage(collect);
		}

		void clear()
		{
			clear([](T&&) {});
		}

		template <typename Collect>
		void collectGarbage(Collect&& collect)
		{
			std::vector<acm::ResourceSlot<T>*> slots = snapshotSlots();
			for (acm::ResourceSlot<T>* slot : slots)
			{
				if (!slot->collect())
					continue;
				if (auto resource = slot->takeResource())
					collect(std::move(*resource));
				std::lock_guard<std::mutex> lock(m_mutex);
				m_free.push_back(slot);
			}
		}

		void collectGarbage()
		{
			collectGarbage([](T&&) {});
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

		std::vector<acm::ResourceSlot<T>*> snapshotSlots()
		{
			std::vector<acm::ResourceSlot<T>*> slots;
			std::lock_guard<std::mutex> lock(m_mutex);
			for (const auto& block : m_blocks)
			{
				for (acm::ResourceSlot<T>& slot : *block)
					slots.push_back(&slot);
			}
			return slots;
		}

		void addBlock()
		{
			auto block = std::make_unique<Block>();
			for (acm::ResourceSlot<T>& slot : *block)
			{
				slot.initialize(m_nextIndex++);
				m_free.push_back(&slot);
			}
			m_blocks.push_back(std::move(block));
		}

		static constexpr size_t BlockSize = 256;
		using Block = std::array<acm::ResourceSlot<T>, BlockSize>;

		std::mutex m_mutex;
		std::vector<std::unique_ptr<Block>> m_blocks;
		std::vector<acm::ResourceSlot<T>*> m_free;
		uint32_t m_nextIndex{0};
	};
} // namespace acm
