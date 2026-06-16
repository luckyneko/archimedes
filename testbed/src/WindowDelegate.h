#pragma once

#include <archimedes/archimedes.h>
#include <memory>
#include <vector>

// Chosen by the delegate from the GPUs/surface support the instance enumerated;
// drives acm::Device + acm::SwapChain creation in Window.
struct SwapChainSettings
{
	uint32_t selectedGPUIdx = 0;
	uint32_t selectedQueueFamilyIdx = 0;
	acm::SurfaceFormat selectedFormat{};
	acm::PresentMode selectedPresentMode{};
};

// The swappable test content. Window owns the window + swapchain lifecycle and
// calls into the delegate; a delegate is where an individual demo/test lives.
class WindowDelegate
{
	public:
		virtual ~WindowDelegate() = default;

		virtual SwapChainSettings onSelectSwapChainSettings(const std::vector<acm::GPU>& gpus, const std::vector<acm::GPUSurfaceSupport>& surfaceSupport) = 0;

		virtual void onInit(acm::Device device, acm::SwapChain swapChain) = 0;
		virtual void onShutdown(acm::Device device, acm::SwapChain swapChain) = 0;
		virtual void onUpdate() = 0;
		virtual void onRender(acm::Device device, acm::SwapChain swapChain) = 0;
};
using WindowDelegatePtr = std::shared_ptr<WindowDelegate>;
