#pragma once

#include "archimedes/acmHandle.h"

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace acm
{
	template <typename T, typename Owner>
	class HandleMap;

	template <typename T, typename Owner>
	class ResourceSlot
	{
	public:
		bool retain(const acm::Handle& handle)
		{
			uint64_t state = m_state.load(std::memory_order_acquire);
			while (matches(handle, state))
			{
				assert(references(state) != UINT32_MAX && "ResourceSlot reference count overflow");
				if (references(state) == UINT32_MAX)
					return false;
				const uint64_t retained = pack(generation(state), references(state) + 1);
				if (m_state.compare_exchange_weak(state, retained, std::memory_order_acq_rel, std::memory_order_acquire))
					return true;
			}
			return false;
		}

		void release(const acm::Handle& handle)
		{
			uint64_t state = m_state.load(std::memory_order_acquire);
			while (matches(handle, state))
			{
				const uint32_t refs = references(state);
				const uint64_t released = refs == 1 ? pack(nextGeneration(generation(state)), 0) : pack(generation(state), refs - 1);
				if (!m_state.compare_exchange_weak(state, released, std::memory_order_acq_rel, std::memory_order_acquire))
					continue;
				if (refs == 1)
					m_map->retire(static_cast<T&>(*this));
				return;
			}
		}

		bool valid(const acm::Handle& handle) const
		{
			return matches(handle, m_state.load(std::memory_order_acquire));
		}

		bool forceInvalidate(const acm::Handle& handle)
		{
			return m_map && m_map->invalidate(static_cast<T&>(*this), handle);
		}

		Owner& owner() const { return *m_owner; }
		uint32_t index() const { return m_index; }
		uint32_t referenceCount() const { return references(m_state.load(std::memory_order_acquire)); }

		void configureSlot(acm::HandleMap<T, Owner>& map, Owner& owner, uint32_t index)
		{
			assert(!m_map && !m_owner);
			m_map = &map;
			m_owner = &owner;
			m_index = index;
		}

		acm::Handle activateSlot()
		{
			const uint64_t state = m_state.load(std::memory_order_relaxed);
			assert(references(state) == 0);
			m_state.store(pack(generation(state), 1), std::memory_order_release);
			return {m_index, generation(state)};
		}

		bool invalidateSlot()
		{
			uint64_t state = m_state.load(std::memory_order_acquire);
			while (references(state) != 0)
			{
				const uint64_t invalid = pack(nextGeneration(generation(state)), 0);
				if (m_state.compare_exchange_weak(state, invalid, std::memory_order_acq_rel, std::memory_order_acquire))
					return true;
			}
			return false;
		}

	protected:
		bool accessible(const acm::Handle& handle) const
		{
			return valid(handle);
		}

	private:
		static_assert(std::atomic<uint64_t>::is_always_lock_free, "Archimedes requires lock-free 64-bit resource state atomics");

		static constexpr uint64_t pack(uint32_t generationValue, uint32_t referenceValue)
		{
			return (uint64_t(generationValue) << 32u) | referenceValue;
		}

		static constexpr uint32_t generation(uint64_t state) { return uint32_t(state >> 32u); }
		static constexpr uint32_t references(uint64_t state) { return uint32_t(state); }

		static uint32_t nextGeneration(uint32_t current)
		{
			assert(current != UINT32_MAX && "ResourceSlot generation overflow");
			return current == UINT32_MAX ? 1u : current + 1u;
		}

		bool matches(const acm::Handle& handle, uint64_t state) const
		{
			return handle.valid() && handle.index() == m_index && handle.generation() == generation(state) && references(state) != 0;
		}

		acm::HandleMap<T, Owner>* m_map{nullptr};
		Owner* m_owner{nullptr};
		uint32_t m_index{0};
		std::atomic<uint64_t> m_state{pack(1, 0)};
	};

	template <typename T, typename Owner>
	class HandleMap
	{
	public:
		struct Inserted
		{
			T* resource{nullptr};
			acm::Handle handle;
		};

		explicit HandleMap(Owner& owner)
			: m_owner(&owner)
		{
		}

		template <typename Constructor>
		Inserted emplace(Constructor&& construct)
		{
			T* resource = acquireSlot();
			if (!construct(*resource))
			{
				resource->retire(*m_owner);
				recycle(*resource);
				return {};
			}
			return {resource, resource->activateSlot()};
		}

		bool invalidate(T& resource, const acm::Handle& handle)
		{
			if (!resource.valid(handle) || !resource.invalidateSlot())
				return false;
			retire(resource);
			return true;
		}

		void clear()
		{
			std::vector<T*> active;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				for (const auto& block : m_blocks)
					for (T& resource : *block)
						if (resource.referenceCount() != 0)
							active.push_back(&resource);
			}
			for (T* resource : active)
				if (resource->invalidateSlot())
					retire(*resource);
		}

		void retire(T& resource)
		{
			resource.retire(*m_owner);
			recycle(resource);
		}

	private:
		void recycle(T& resource)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_free.push_back(&resource);
		}
		static constexpr size_t BlockSize = 256;
		using Block = std::array<T, BlockSize>;

		T* acquireSlot()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_free.empty())
				addBlock();
			T* resource = m_free.back();
			m_free.pop_back();
			return resource;
		}

		void addBlock()
		{
			auto block = std::make_unique<Block>();
			for (T& resource : *block)
			{
				resource.configureSlot(*this, *m_owner, m_nextIndex++);
				m_free.push_back(&resource);
			}
			m_blocks.push_back(std::move(block));
		}

		Owner* m_owner;
		std::mutex m_mutex;
		std::vector<std::unique_ptr<Block>> m_blocks;
		std::vector<T*> m_free;
		uint32_t m_nextIndex{0};
	};
} // namespace acm
