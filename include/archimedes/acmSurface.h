#pragma once

#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"

namespace acm
{
    class Surface
    {
        public:
            Surface() {}
            Surface(acm::Instance instance, VkSurfaceKHR surface);

            inline void reset() { m.reset(); }
            inline bool valid() const { return m != nullptr; }

            const std::vector<acm::GPUSurfaceSupport>& getGPUSupport() const;
            VkSurfaceKHR vkSurface();

        private:
            struct impl; std::shared_ptr<impl> m;
    };
}