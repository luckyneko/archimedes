#include "Window.h"
#include "MainDelegate.h"

#include <archimedes/archimedes.h>
#include <spdlog/spdlog.h>

static const char* APP_NAME = "Archimedes Testbed";
static const acm::Version APP_VERSION = {0, 1, 0, 0};
static const int APP_WIDTH = 1280;
static const int APP_HEIGHT = 720;

#ifndef NDEBUG
	static const bool APP_DEBUG = true;
#else
	static const bool APP_DEBUG = false;
#endif

int main(int /*argc*/, char* /*argv*/[])
{
#ifndef NDEBUG
	spdlog::set_level(spdlog::level::debug);
#endif

	spdlog::info("{0} v{1}.{2}.{3} {4}",
		APP_NAME,
		APP_VERSION.major, APP_VERSION.minor, APP_VERSION.patch,
		APP_DEBUG ? "DEBUG" : "RELEASE");

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
