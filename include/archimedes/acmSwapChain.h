#pragma once

#include "archimedes/acmForward.h"
#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"

namespace acm
{
    class SwapChain
    {
        public:
            SwapChain() {}
            // desiredExtent is only used when the surface defers sizing to the
            // app (currentExtent == UINT32_MAX, e.g. headless); it is clamped to
            // the surface's min/max. A window-backed surface ignores it.
            SwapChain(acm::Device device, acm::Surface surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent = {});

            inline void reset() { m.reset(); }
            inline bool valid() const { return m != nullptr; }

            VkSwapchainKHR vkSwapChain();
            acm::SurfaceFormat getFormat() const;
            acm::Extent2D getExtents() const;
            size_t getRenderTargetCount() const;
            acm::RenderTarget getRenderTarget(size_t idx) const;

        private:
            struct impl; std::shared_ptr<impl> m;
    };
}
