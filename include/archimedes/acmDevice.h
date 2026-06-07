#pragma once

#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include <vulkan/vulkan.h>

namespace acm
{
    class Device
    {
        public:
            Device() {}
            Device(acm::Instance instance, const acm::GPU& gpu, uint32_t queueIdx);

            inline void reset() { m.reset(); }
            inline bool valid() const { return m != nullptr; }

            const acm::GPU& getGPU() const;
            uint32_t getQueueIdx() const;
            VkDevice vkDevice();
            VkQueue vkQueue();

        private:
            struct impl; std::shared_ptr<impl> m;
    };
}
