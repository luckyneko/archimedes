/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include <archimedes/acmVersion.h>
#include <catch2/catch_all.hpp>

TEST_CASE("Version - default construction yields 0.0.0", "[version]")
{
	acm::Version v{};
	REQUIRE(int(v.major) == 0);
	REQUIRE(int(v.minor) == 0);
	REQUIRE(int(v.patch) == 0);
}

TEST_CASE("Version - fields round-trip", "[version]")
{
	acm::Version v{1, 2, 334};
	REQUIRE(int(v.major) == 1);
	REQUIRE(int(v.minor) == 2);
	REQUIRE(int(v.patch) == 334);
}

TEST_CASE("Version - engine acm::VERSION is 0.1.0", "[version]")
{
	REQUIRE(int(acm::VERSION.major) == 0);
	REQUIRE(int(acm::VERSION.minor) == 1);
	REQUIRE(int(acm::VERSION.patch) == 0);
}
