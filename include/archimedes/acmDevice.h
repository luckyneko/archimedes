#pragma once

#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmVkFwd.h"
#include <cstdint>
#include <functional>

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

            // Deferred destruction. A Vulkan object must outlive every GPU
            // submission that references it, which the CPU-side handle refcount
            // cannot know about. Resources therefore enqueue their teardown here
            // (tagged with the current frame) instead of destroying inline; the
            // render loop calls beginFrame() once per frame and collectGarbage()
            // with the newest frame index known to have fully retired on the GPU.
            // Not thread-safe (single render thread assumed); guard the queue if
            // that ever changes. The Device destructor waits idle then flushes
            // everything still pending.
            void beginFrame();
            uint64_t currentFrame() const;
            void enqueueDestroy(std::function<void()> destroy);
            void collectGarbage(uint64_t completedFrame);

        private:
            struct impl; std::shared_ptr<impl> m;
    };
}
