#pragma once

#include "archimedes/acmForward.h"
#include "archimedes/acmVkFwd.h"
#include <vector>

namespace acm
{
    class RenderTarget
    {
        public:
            RenderTarget() {}
            RenderTarget(acm::Device device, const std::vector<acm::Image>& images);

            inline void reset() { m.reset(); }
            inline bool valid() const { return m != nullptr; }

            VkRenderPass vkRenderPass();
            VkFramebuffer vkFramebuffer();

        private:
            struct impl; std::shared_ptr<impl> m;
    };
}
