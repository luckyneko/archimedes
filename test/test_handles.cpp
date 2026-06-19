#include <archimedes/archimedes.h>
#include <catch2/catch_all.hpp>

// Pure handle semantics — no Vulkan device required. Every acm:: type is a
// value handle over a shared_ptr pImpl: default-constructed is null/invalid.

TEST_CASE("default-constructed handles are invalid", "[acm][handle]")
{
	REQUIRE_FALSE(acm::Instance().valid());
	REQUIRE_FALSE(acm::Device().valid());
	REQUIRE_FALSE(acm::Surface().valid());
	REQUIRE_FALSE(acm::SwapChain().valid());
	REQUIRE_FALSE(acm::RenderTarget().valid());
	REQUIRE_FALSE(acm::Shader().valid());
	REQUIRE_FALSE(acm::Pipeline().valid());
	REQUIRE_FALSE(acm::CommandPool().valid());
	REQUIRE_FALSE(acm::CommandBuffer().valid());
	REQUIRE_FALSE(acm::Renderer().valid());
	REQUIRE_FALSE(acm::Texture().valid());
	REQUIRE_FALSE(acm::Buffer().valid());
	REQUIRE_FALSE(acm::Sampler().valid());
	REQUIRE_FALSE(acm::DescriptorSetLayout().valid());
	REQUIRE_FALSE(acm::DescriptorSet().valid());
	REQUIRE_FALSE(acm::UniformRing().valid());
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
