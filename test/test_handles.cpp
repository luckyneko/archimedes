#include <catch2/catch_all.hpp>
#include <archimedes/archimedes.h>

// Pure handle semantics — no Vulkan device required. Every acm:: type is a
// value handle over a shared_ptr pImpl: default-constructed is null/invalid.

TEST_CASE("default-constructed handles are invalid", "[acm][handle]")
{
	REQUIRE_FALSE(acm::Instance().valid());
	REQUIRE_FALSE(acm::Device().valid());
	REQUIRE_FALSE(acm::Surface().valid());
	REQUIRE_FALSE(acm::SwapChain().valid());
	REQUIRE_FALSE(acm::Image().valid());
	REQUIRE_FALSE(acm::RenderTarget().valid());
}

TEST_CASE("reset on a null handle stays invalid", "[acm][handle]")
{
	acm::Instance i;
	i.reset();
	REQUIRE_FALSE(i.valid());
}

TEST_CASE("copying a null handle yields another null handle", "[acm][handle]")
{
	acm::Device a;
	acm::Device b = a;
	REQUIRE_FALSE(a.valid());
	REQUIRE_FALSE(b.valid());
}
