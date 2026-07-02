/*
 *  Created by LuckyNeko on 30/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmResourceSlot.h"

namespace acm
{
	template <typename T>
	class ResourcePool;

	template <typename T>
	class ResourceRef
	{
		using ID = acm::ResourceSlotID<T>;

	public:
		ResourceRef() = default;
		ResourceRef(const ResourceRef& other)
			: m_slot(other.m_slot)
			, m_id(other.m_id)
		{
			if (m_slot && !m_slot->retain(m_id))
				clear();
		}

		ResourceRef& operator=(const ResourceRef& other)
		{
			if (this == &other)
				return *this;
			reset();
			m_slot = other.m_slot;
			m_id = other.m_id;
			if (m_slot && !m_slot->retain(m_id))
				clear();
			return *this;
		}

		ResourceRef(ResourceRef&& other) noexcept
			: m_slot(other.m_slot)
			, m_id(other.m_id)
		{
			other.clear();
		}

		ResourceRef& operator=(ResourceRef&& other) noexcept
		{
			if (this == &other)
				return *this;
			reset();
			m_slot = other.m_slot;
			m_id = other.m_id;
			other.clear();
			return *this;
		}

		~ResourceRef() { reset(); }

		void reset()
		{
			if (m_slot)
				m_slot->release(m_id);
			clear();
		}

		bool valid() const { return m_slot && m_slot->valid(m_id); }
		T* access() const { return valid() ? &m_slot->resource() : nullptr; }
		explicit operator bool() const { return valid(); }
		const ID& id() const { return m_id; }
		bool forceInvalidate() { return m_slot && m_slot->forceRetire(m_id); }

	private:
		friend class acm::ResourcePool<T>;
		ResourceRef(acm::ResourceSlot<T>* slot, ID id)
			: m_slot(slot)
			, m_id(id)
		{
		}

		void clear()
		{
			m_slot = nullptr;
			m_id = {};
		}

		acm::ResourceSlot<T>* m_slot{nullptr};
		ID m_id;
	};
} // namespace acm
