#include "archimedes/acmUtils.h"
#include "archimedes/archimedes.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace acm::Utils
{
    void logGPUSupport(acm::Instance instance, acm::Surface surface)
    {
        const auto& gpus = instance.getAvailableGPUs();
        const auto& surfaceSupport = surface.getGPUSupport();
        spdlog::debug("GPUs Found: {0}", gpus.size());
        for(auto& gpu : gpus)
        {
            auto gpuSupportIt = std::find_if(surfaceSupport.begin(), surfaceSupport.end(), 
                [targetIdx = gpu.index](const acm::GPUSurfaceSupport& i){ return i.gpuIndex == targetIdx; });
            const acm::GPUSurfaceSupport* gpuSupport = (gpuSupportIt != surfaceSupport.end()) ? &*gpuSupportIt : nullptr;
            
            spdlog::debug("{0} - {1}", gpu.index, gpu.properties.deviceName);

            spdlog::debug("  QueueFamilies:");
            for(auto& queueFamily : gpu.queueFamilies)
            {
                bool supportsPresent = (gpuSupport) ? gpuSupport->queueFamilySupportsPresent[queueFamily.index] : false;
                spdlog::debug("    {0} - {1}{2}{3}{4} ({5})", 
                    queueFamily.index,
                    queueFamily.supportsGraphics ? "G" : "-",
                    queueFamily.supportsCompute  ? "C" : "-",
                    queueFamily.supportsTransfer ? "T" : "-",
                    supportsPresent ? "P" : "-",
                    queueFamily.queueCount
                );
            }

            if(gpuSupport)
            {
                spdlog::debug("  SurfaceFormats:");
                for(auto& format : gpuSupport->supportedFormats)
                {
                    spdlog::debug("    {0} {1}", int(format.format), int(format.colorSpace));
                }
                spdlog::debug("  PresentModes:");
                for(auto& presMode : gpuSupport->supportedPresentModes)
                {
                    spdlog::debug("    {0}", int(presMode));
                }
            }
        }
    }
}