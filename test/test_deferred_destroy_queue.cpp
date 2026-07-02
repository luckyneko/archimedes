/*
 *  Created by LuckyNeko on 01/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/DeferredDestroyQueue.h"

#include <catch2/catch_all.hpp>

#include <memory>
#include <utility>

namespace acmtest
{
	class DeferredResource
	{
	public:
		explicit DeferredResource(std::shared_ptr<bool> destroyed)
			: m_destroyed(std::move(destroyed))
		{
		}

		~DeferredResource()
		{
			if (m_destroyed)
				*m_destroyed = true;
		}

		DeferredResource(const DeferredResource&) = delete;
		DeferredResource& operator=(const DeferredResource&) = delete;
		DeferredResource(DeferredResource&&) noexcept = default;
		DeferredResource& operator=(DeferredResource&&) noexcept = default;

	private:
		std::shared_ptr<bool> m_destroyed;
	};
} // namespace acmtest

TEST_CASE("DeferredDestroyQueue stores move-only resources until their submission completes", "[acm][unit]")
{
	acm::DeferredDestroyQueue queue;
	auto destroyed = std::make_shared<bool>(false);
	auto resource = acmtest::DeferredResource(destroyed);

	queue.enqueue(3, std::move(resource));

	REQUIRE_FALSE(queue.empty());
	queue.collect(2);
	REQUIRE_FALSE(*destroyed);
	REQUIRE_FALSE(queue.empty());

	queue.collect(3);
	REQUIRE(*destroyed);
	REQUIRE(queue.empty());
}
