#pragma once

#include "archimedes/acmForward.h"
#include <vulkan/vulkan.h>

namespace acm
{
    class SwapChain
    {
        public:
            SwapChain() {}
            SwapChain(acm::Device device, acm::Surface surface, VkSurfaceFormatKHR format, VkPresentModeKHR presentMode);
            
            inline void reset() { m.reset(); }
            inline bool valid() const { return m != nullptr; }

            VkSwapchainKHR vkSwapChain();
            VkSurfaceFormatKHR getFormat() const;
            VkExtent2D getExtents() const;
            size_t getRenderTargetCount() const;
            acm::RenderTarget getRenderTarget(size_t idx) const;

        private:
            struct impl; std::shared_ptr<impl> m;
    };
}
