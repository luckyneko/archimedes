/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <cstdint>

namespace acm
{
	struct Version
	{
		uint8_t major{0};
		uint8_t minor{0};
		uint16_t patch{0};
	};
	static const acm::Version VERSION = {0, 1, 0};
} // namespace acm
