#include "Window.h"
#include "MainDelegate.h"

#include <archimedes/archimedes.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#if defined(__APPLE__)
	#include <mach-o/dyld.h> // _NSGetExecutablePath
#endif

static const char* APP_NAME = "Archimedes Testbed";
static const acm::Version APP_VERSION = {0, 1, 0, 0};
static const int APP_WIDTH = 1280;
static const int APP_HEIGHT = 720;

#ifndef NDEBUG
	static const bool APP_DEBUG = true;
#else
	static const bool APP_DEBUG = false;
#endif

namespace
{
	// acm_stage_vulkan_runtime() copies the vendored MoltenVK ICD next to the
	// executable in vulkan/. The generated run_<target>.sh exports
	// VK_ICD_FILENAMES so the loader finds it, but launching the binary directly
	// (e.g. from the IDE) skips that and the loader reports "Found no drivers!".
	// Point it at the staged ICD here unless the caller already set one, so a
	// direct launch works while run_<target>.sh / an explicit override still win.
	void useStagedVulkanICD()
	{
#if defined(__APPLE__)
		if(std::getenv("VK_ICD_FILENAMES"))
			return;

		uint32_t size = 0;
		_NSGetExecutablePath(nullptr, &size); // first call reports required size
		std::string pathBuf(size, '\0');
		if(_NSGetExecutablePath(pathBuf.data(), &size) != 0)
			return;

		std::error_code ec;
		std::filesystem::path exe = std::filesystem::canonical(pathBuf.c_str(), ec);
		if(ec)
			return;

		const std::filesystem::path icd = exe.parent_path() / "vulkan" / "MoltenVK_icd.json";
		if(std::filesystem::exists(icd))
			setenv("VK_ICD_FILENAMES", icd.string().c_str(), 0); // 0: don't overwrite
#endif
	}
}

int main(int /*argc*/, char* /*argv*/[])
{
#ifndef NDEBUG
	spdlog::set_level(spdlog::level::debug);
#endif

	spdlog::info("{0} v{1}.{2}.{3} {4}",
		APP_NAME,
		APP_VERSION.major, APP_VERSION.minor, APP_VERSION.patch,
		APP_DEBUG ? "DEBUG" : "RELEASE");

	useStagedVulkanICD();

	bool ok = true;

	// ACM Instance
	acm::Instance instance;
	if(ok)
	{
		instance = acm::Instance(APP_NAME, APP_VERSION);
		ok = instance.valid();
		spdlog::info("CreateACMInstance: {0}", ok ? "ok" : "FAIL");
	}

	// Window (creates surface, device, swapchain via the delegate)
	Window window;
	if(ok)
	{
		window = Window(std::make_shared<MainDelegate>(), instance, APP_NAME, APP_WIDTH, APP_HEIGHT);
		ok = window.valid();
		spdlog::info("CreateWindow: {0}", ok ? "ok" : "FAIL");
	}

	// Run loop
	if(ok)
	{
		spdlog::info("Running... (close the window to exit)");
		while(window.valid())
		{
			window.update();
			window.render();
		}
	}

	return ok ? 0 : 1;
}
