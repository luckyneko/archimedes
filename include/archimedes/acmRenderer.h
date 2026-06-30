#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"
#include "archimedes/acmNative.h"

#include <cstdint>
#include <functional>

namespace acm
{
	class Renderer
	{
	public:
		static constexpr uint32_t MaxFramesInFlight = 2;

		Renderer();
		Renderer(const acm::Renderer& other);
		Renderer& operator=(const acm::Renderer& other);
		Renderer(acm::Renderer&& other) noexcept;
		Renderer& operator=(acm::Renderer&& other) noexcept;
		~Renderer();

		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::native::Renderer* native() const;

		acm::Error render(const std::function<void(acm::CommandBuffer& cmd, uint32_t frameIndex)>& record);
		acm::Error render(const std::function<void(acm::CommandBuffer& cmd, uint32_t frameIndex)>& prePass,
						  const std::function<void(acm::CommandBuffer& cmd, uint32_t frameIndex)>& record);

	private:
		friend acm::native::Device;
		Renderer(acm::ResourceRef<acm::native::Renderer> resource);
		explicit Renderer(acm::Error error);

		acm::ResourceRef<acm::native::Renderer> m_resource;
		acm::Error m_error;
	};
} // namespace acm
