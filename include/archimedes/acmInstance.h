#pragma once

#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmVersion.h"
#include "archimedes/acmVkFwd.h"
#include <vector>

namespace acm
{
    class Instance
    {
        public:
            Instance() {}
            Instance(const char* appName, const acm::Version& appVer);

            inline void reset() { m.reset(); }
            inline bool valid() const { return m != nullptr; }

            const std::vector<const char *>& getLayerNames() const;
            const std::vector<acm::GPU>& getAvailableGPUs() const;
            VkInstance vkInstance();

        private:
            struct impl; std::shared_ptr<impl> m;
    };
}
