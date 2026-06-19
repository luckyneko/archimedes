#pragma once

#include "RenderContext.h"
#include <archimedes/archimedes.h>
#include <string>
#include <vector>

// One window an example wants opened.
struct WindowSpec
{
	std::string title{"Archimedes"};
	int width{800};
	int height{600};
	int posX{120};
	int posY{160};
};

// What an example asks the App to set up: its windows (1..N) and the swapchain settings
// shared by all of them.
struct ExampleConfig
{
	std::vector<WindowSpec> windows;
	bool depth{true};
	acm::SampleCount samples{acm::SampleCount::Four};
};

// A swappable testbed demo. The App owns the GLFW windows + surfaces + device + a
// RenderContext per window, and drives the example: onInit once, then each frame
// onUpdate (main thread) followed by onRenderView per window (which may run on a worker
// thread for multi-window examples — so it must touch only Vulkan, no GLFW). An example
// composes the acm:: API to exercise a slice of the renderer; collectively they give the
// testbed live-driver coverage of the pieces the headless tests can't reach.
class Example
{
public:
	virtual ~Example() = default;

	// The windows + swapchain settings to create. Called once, before onInit.
	virtual ExampleConfig config() = 0;

	// Build pipelines / scene / descriptors. `views` has one RenderContext per window
	// (already holding a swapchain + renderer), borrowed for the example's lifetime.
	virtual bool onInit(acm::Device device, const std::vector<RenderContext*>& views) = 0;

	// Per-frame work shared across windows, on the main thread (e.g. a compute dispatch,
	// advancing animation). `time` is a fixed-step clock.
	virtual void onUpdate(acm::Device device, float time) = 0;

	// Record + present one window's frame. May run on a worker thread (multi-window), so
	// it must touch only Vulkan, never GLFW.
	virtual void onRenderView(uint32_t viewIndex, float time) = 0;

	// Release the example's GPU resources (before the App destroys the device).
	virtual void onShutdown() = 0;
};
