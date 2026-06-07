#pragma once

#include "WindowDelegate.h"
#include <archimedes/acmForward.h>
#include <memory>
#include <string>

// Owns the GLFW window + the acm::Surface/Device/SwapChain lifecycle, driving a
// WindowDelegate for the actual per-frame work. Same handle shape as acm::.
class Window
{
	public:
		Window() {}
		Window(WindowDelegatePtr delegate, acm::Instance instance, const std::string& name, int width, int height);

		inline void reset() { m.reset(); }
		inline bool valid() const { return m != nullptr; }

		void update();
		void render();

	private:
		struct impl; std::shared_ptr<impl> m;
};
