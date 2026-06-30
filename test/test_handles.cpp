#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <type_traits>
#include <utility>

// Pure resource wrapper semantics -- no Vulkan device required. Every acm:: resource is
// a value handle: default-constructed is null/invalid regardless of its backend.

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
}

TEST_CASE("reset on a null handle stays invalid", "[acm][handle]")
{
	acm::Instance i;
	i.reset();
	REQUIRE_FALSE(i.valid());
}

TEST_CASE("copying a null handle yields another null handle", "[acm][handle]")
{
	acm::Buffer a;
	acm::Buffer b = a;
	REQUIRE_FALSE(a.valid());
	REQUIRE_FALSE(b.valid());
}

TEST_CASE("owning roots are move-only", "[acm][handle]")
{
	STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<acm::Instance>);
	STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<acm::Instance>);
	STATIC_REQUIRE(std::is_nothrow_move_constructible_v<acm::Instance>);
	STATIC_REQUIRE(std::is_nothrow_move_assignable_v<acm::Instance>);
	STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<acm::Device>);
	STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<acm::Device>);
	STATIC_REQUIRE(std::is_nothrow_move_constructible_v<acm::Device>);
	STATIC_REQUIRE(std::is_nothrow_move_assignable_v<acm::Device>);

	acm::Device source;
	acm::Device destination = std::move(source);
	REQUIRE_FALSE(source.valid());
	REQUIRE_FALSE(destination.valid());
}
