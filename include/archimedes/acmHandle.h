#pragma once

#include <cstdint>

namespace acm
{
	class Handle
	{
	public:
		Handle() = default;
		Handle(uint32_t index, uint32_t generation)
			: m_index(index)
			, m_generation(generation)
		{
		}

		void reset()
		{
			m_index = InvalidIndex;
			m_generation = 0;
		}

		bool valid() const { return m_index != InvalidIndex; }
		uint32_t index() const { return m_index; }
		uint32_t generation() const { return m_generation; }

	private:
		static constexpr uint32_t InvalidIndex = UINT32_MAX;

		uint32_t m_index{InvalidIndex};
		uint32_t m_generation{0};
	};
} // namespace acm
