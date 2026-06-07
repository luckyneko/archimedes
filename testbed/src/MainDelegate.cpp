#include "MainDelegate.h"

#include <archimedes/archimedes.h>
#include <spdlog/spdlog.h>
#include <algorithm>

SwapChainSettings MainDelegate::onSelectSwapChainSettings(const std::vector<acm::GPU>& gpus, const std::vector<acm::GPUSurfaceSupport>& surfaceSupport)
{
	SwapChainSettings result;
	for(const auto& gpu : gpus)
	{
		// Must have surface support at all
		auto gpuSupportIt = std::find_if(surfaceSupport.begin(), surfaceSupport.end(),
			[targetIdx = gpu.index](const acm::GPUSurfaceSupport& i) { return i.gpuIndex == targetIdx; });
		if(gpuSupportIt == surfaceSupport.end())
		{
			spdlog::debug("no surface support for: {0}", gpu.properties.deviceName);
			continue;
		}
		const acm::GPUSurfaceSupport& gpuSupport = *gpuSupportIt;

		// Must expose at least one format + present mode
		if(gpuSupport.supportedFormats.empty() || gpuSupport.supportedPresentModes.empty())
		{
			spdlog::debug("no surface format/mode for: {0}", gpu.properties.deviceName);
			continue;
		}

		// First queue family that supports graphics + present wins
		for(const auto& queueFamily : gpu.queueFamilies)
		{
			bool supportsPresent = gpuSupport.queueFamilySupportsPresent[queueFamily.index];
			if(queueFamily.supportsGraphics && supportsPresent)
			{
				result.selectedGPUIdx = gpu.index;
				result.selectedQueueFamilyIdx = queueFamily.index;
				result.selectedFormat = gpuSupport.supportedFormats[0];
				result.selectedPresentMode = gpuSupport.supportedPresentModes[0];
				return result;
			}
		}
		spdlog::debug("no graphics+present queue for: {0}", gpu.properties.deviceName);
	}

	return result;
}

void MainDelegate::onInit(acm::Device, acm::SwapChain swapChain)
{
	// Stage (a): nothing rendered yet — just prove the swapchain is live.
	spdlog::info("MainDelegate::onInit — swapchain render targets: {0}", swapChain.getRenderTargetCount());
}

void MainDelegate::onShutdown(acm::Device, acm::SwapChain)
{
}

void MainDelegate::onUpdate()
{
}

void MainDelegate::onRender(acm::Device, acm::SwapChain)
{
}
