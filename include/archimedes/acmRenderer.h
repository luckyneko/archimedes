#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmVkFwd.h"
#include <cstdint>
#include <functional>

namespace acm
{
	// The per-frame driver: owns the command pool/buffers and the MaxFramesInFlight
	// semaphores + fences, and runs the acquire -> record -> submit -> present loop.
	class Renderer
	{
	public:
		// How many frames the renderer keeps in flight, and therefore how many slots
		// the per-frame index passed to record(...) cycles through. Size any per-frame
		// resource ring (e.g. dynamic uniform buffers) to this.
		static constexpr uint32_t MaxFramesInFlight = 2;

		Renderer() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		// Drives one frame: waits on this slot's fence, collects retired garbage,
		// acquires the next swapchain image, begins a command buffer + its render
		// pass, calls record(cmd, frameIndex) to register draw calls, then ends,
		// submits, and presents. The callback only records draws into the supplied
		// CommandBuffer (the render pass is already begun). `frameIndex` is the
		// frame-in-flight slot (0..MaxFramesInFlight-1) the renderer just waited on,
		// so per-slot resources (a ring of uniform buffers) are free to rewrite.
		// Returns a non-ok Error if the submit fails; returns an ok Error on all
		// other paths (including swapchain recreation and minimized skips).
		acm::Error render(const std::function<void(acm::CommandBuffer cmd, uint32_t frameIndex)>& record);

		// As above, but `prePass` records into the same command buffer *before* the render
		// pass begins (and outside it): compute dispatches, barriers, image transitions —
		// work that can't run inside a render pass. `record` then records draws inside the
		// pass as usual. Because it's one command buffer, a compute write in `prePass`
		// feeds the draws through a barrier with no extra submit. `frameIndex` is the same
		// frame-in-flight slot for both callbacks.
		acm::Error render(const std::function<void(acm::CommandBuffer cmd, uint32_t frameIndex)>& prePass,
						  const std::function<void(acm::CommandBuffer cmd, uint32_t frameIndex)>& record);

	private:
		friend class Device; // only Device::createRenderer builds one
		Renderer(acm::Device device, acm::SwapChain swapChain);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
