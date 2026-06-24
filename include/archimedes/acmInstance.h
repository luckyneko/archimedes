#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmNative.h"
#include "archimedes/acmVersion.h"

#include <memory>
#include <vector>

namespace acm
{
	class Instance
	{
	public:
		Instance();
		Instance(const char* appName, const acm::Version& appVer);
		Instance(const acm::Instance& other) = delete;
		Instance& operator=(const acm::Instance& other) = delete;
		Instance(acm::Instance&& other) noexcept;
		Instance& operator=(acm::Instance&& other) noexcept;
		~Instance();

		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::native::InstanceHandle nativeInstance() const;

		acm::Surface createSurface(acm::native::SurfaceHandle surface);
		acm::Device createDevice(const acm::GPU& gpu, uint32_t queueIdx);
		const std::vector<acm::GPU>& getAvailableGPUs() const;

	private:
		std::unique_ptr<acm::native::Instance> m;
		acm::Error m_error;
	};
} // namespace acm
