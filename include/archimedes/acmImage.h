#pragma once

#include "archimedes/acmForward.h"
#include <vulkan/vulkan.h>

namespace acm
{
    class Image
    {
        public:
            Image() {}
            Image(acm::Device device, VkImage image, VkImageCreateInfo info);

            inline void reset() { m.reset(); }
            inline bool valid() const { return m != nullptr; }

            VkImageType type() const;
            VkFormat format() const;
            VkExtent3D extent() const;
            VkImage vkImage() const;

        private:
            struct impl; std::shared_ptr<impl> m;
    };
}
