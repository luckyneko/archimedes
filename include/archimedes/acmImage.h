#pragma once

#include "archimedes/acmForward.h"
#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"

namespace acm
{
    class Image
    {
        public:
            Image() {}
            Image(acm::Device device, VkImage image, const acm::ImageDesc& desc);

            inline void reset() { m.reset(); }
            inline bool valid() const { return m != nullptr; }

            acm::ImageType type() const;
            acm::Format format() const;
            acm::Extent3D extent() const;
            VkImage vkImage() const;

        private:
            struct impl; std::shared_ptr<impl> m;
    };
}
