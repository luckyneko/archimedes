#pragma once

#include "WindowDelegate.h"

// The testbed's main demo. Stage (a): selects a graphics+present capable
// GPU/queue and a surface format; rendering hooks are stubs for now and will
// grow into a triangle in stage (b).
class MainDelegate : public WindowDelegate
{
	public:
		SwapChainSettings onSelectSwapChainSettings(const std::vector<acm::GPU>& gpus, const std::vector<acm::GPUSurfaceSupport>& surfaceSupport) final;

		void onInit(acm::Device device, acm::SwapChain swapChain) final;
		void onShutdown(acm::Device device, acm::SwapChain swapChain) final;
		void onUpdate() final;
		void onRender(acm::Device device, acm::SwapChain swapChain) final;
};
