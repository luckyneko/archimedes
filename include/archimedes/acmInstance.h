#pragma once

#include "archimedes/acmError.h"
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

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		// Factories — the only way to build children of an Instance.
		acm::Surface createSurface(VkSurfaceKHR surface);
		acm::Device createDevice(const acm::GPU& gpu, uint32_t queueIdx);

		const std::vector<const char*>& getLayerNames() const;
		const std::vector<acm::GPU>& getAvailableGPUs() const;
		VkInstance vkInstance();

	private:
		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
