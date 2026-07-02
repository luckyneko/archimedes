#include "App.h"

#include "Example.h"
#include "RenderContext.h"
#include "RenderWorker.h"

#include <archimedes/archimedes.h>
#include <archimedes/vulkan/RuntimeEnv.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace
{
	const char* APP_NAME = "Archimedes Testbed";
	const acm::Version APP_VERSION = {0, 1, 0};

	// Create a GLFW window (Vulkan, no OpenGL context) + its acm::Surface. GLFW window
	// creation must happen on the main thread; the surface is needed before the shared
	// device can be created (present-queue selection needs every window's surface).
	acm::Surface createWindowSurface(acm::Instance& instance, const WindowSpec& spec, GLFWwindow*& outWindow)
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // acm::Renderer rebuilds the swapchain on resize
		GLFWwindow* window = glfwCreateWindow(spec.width, spec.height, spec.title.c_str(), nullptr, nullptr);
		if (!window)
			return {};
		glfwSetWindowPos(window, spec.posX, spec.posY);

		acm::native::SurfaceHandle nativeSurface{};
		if (glfwCreateWindowSurface(instance.nativeInstance(), window, nullptr, &nativeSurface) != VK_SUCCESS)
		{
			fprintf(stderr, "glfwCreateWindowSurface failed\n");
			glfwDestroyWindow(window);
			return {};
		}
		outWindow = window;
		return instance.createSurface(nativeSurface);
	}

	// Pick a GPU + queue family that supports graphics and can present to *every* window
	// surface, plus a format/present mode. (A windowing system's present queue typically
	// serves all its surfaces, but we check each.)
	struct Selection
	{
		uint32_t gpuIdx{0};
		uint32_t queueIdx{0};
		acm::SurfaceFormat format;
		acm::PresentMode presentMode{acm::PresentMode::Fifo};
		bool ok{false};
	};

	Selection selectSettings(const acm::Instance& instance, const std::vector<acm::Surface>& surfaces)
	{
		for (const acm::GPU& gpu : instance.getAvailableGPUs())
		{
			std::vector<const acm::GPUSurfaceSupport*> support(surfaces.size(), nullptr);
			bool allSupported = true;
			for (size_t i = 0; i < surfaces.size(); ++i)
			{
				const auto& list = surfaces[i].getGPUSupport();
				auto it = std::find_if(list.begin(), list.end(),
									   [idx = gpu.index](const acm::GPUSurfaceSupport& s)
									   { return s.gpuIndex == idx; });
				if (it == list.end() || it->supportedFormats.empty() || it->supportedPresentModes.empty())
				{
					allSupported = false;
					break;
				}
				support[i] = &*it;
			}
			if (!allSupported)
				continue;

			for (const acm::GPUQueueFamily& qf : gpu.queueFamilies)
			{
				if (!qf.supportsGraphics)
					continue;
				bool presentsAll = true;
				for (const acm::GPUSurfaceSupport* s : support)
					if (qf.index >= s->queueFamilySupportsPresent.size() || !s->queueFamilySupportsPresent[qf.index])
					{
						presentsAll = false;
						break;
					}
				if (!presentsAll)
					continue;

				Selection sel;
				sel.gpuIdx = gpu.index;
				sel.queueIdx = qf.index;
				sel.format = support[0]->supportedFormats[0];
				sel.presentMode = support[0]->supportedPresentModes[0];
				for (acm::PresentMode pm : support[0]->supportedPresentModes)
					if (pm == acm::PresentMode::Fifo) // vsync: cap frames to the refresh rate
					{
						sel.presentMode = pm;
						break;
					}
				sel.ok = true;
				return sel;
			}
		}
		return {};
	}

	bool anyWindowClosing(const std::vector<GLFWwindow*>& windows)
	{
		for (GLFWwindow* w : windows)
			if (glfwWindowShouldClose(w))
				return true;
		return false;
	}
} // namespace

int App::run(Example& example)
{
	acm::vulkan::useStagedVulkanICD();

	acm::InstanceConfig instanceConfig;
	instanceConfig.validation = true;
	instanceConfig.debug = true;
	acm::Instance instance(APP_NAME, APP_VERSION, instanceConfig);
	if (!instance.valid())
	{
		fprintf(stderr, "CreateACMInstance: FAIL\n");
		return 1;
	}

	// GLFW (all window/event calls stay on this main thread).
	glfwInitVulkanLoader(reinterpret_cast<PFN_vkGetInstanceProcAddr>(vkGetInstanceProcAddr));
	if (glfwInit() != GLFW_TRUE || glfwVulkanSupported() != GLFW_TRUE)
	{
		fprintf(stderr, "GLFW init / Vulkan support: FAIL\n");
		return 1;
	}

	const ExampleConfig cfg = example.config();
	const size_t viewCount = cfg.windows.size();
	if (viewCount == 0)
	{
		fprintf(stderr, "example requested no windows\n");
		glfwTerminate();
		return 1;
	}

	// Windows + surfaces (surfaces must exist before the shared device).
	std::vector<GLFWwindow*> windows(viewCount, nullptr);
	std::vector<acm::Surface> surfaces;
	for (size_t i = 0; i < viewCount; ++i)
		surfaces.push_back(createWindowSurface(instance, cfg.windows[i], windows[i]));
	for (const acm::Surface& s : surfaces)
		if (!s.valid())
		{
			fprintf(stderr, "CreateACMSurface: FAIL\n");
			return 1;
		}

	// One shared device for every window.
	const Selection sel = selectSettings(instance, surfaces);
	if (!sel.ok)
	{
		fprintf(stderr, "no GPU presents to all windows\n");
		return 1;
	}
	acm::Device device = instance.createDevice(instance.getAvailableGPUs()[sel.gpuIdx], sel.queueIdx);
	if (!device.valid())
	{
		fprintf(stderr, "CreateACMDevice: FAIL\n");
		return 1;
	}
	printf("SelectedGPU: %s\n", instance.getAvailableGPUs()[sel.gpuIdx].name.c_str());

	// A RenderContext per window. Sized up front so the pointers handed to the example
	// (and the workers) stay stable.
	std::vector<RenderContext> contexts(viewCount);
	std::vector<RenderContext*> contextPtrs(viewCount, nullptr);
	for (size_t i = 0; i < viewCount; ++i)
	{
		int fbW = 0, fbH = 0;
		glfwGetFramebufferSize(windows[i], &fbW, &fbH);
		if (!contexts[i].init(device, surfaces[i], sel.format, sel.presentMode, cfg.depth, cfg.samples, acm::Extent2D{uint32_t(fbW), uint32_t(fbH)}))
		{
			fprintf(stderr, "RenderContext init: FAIL\n");
			return 1;
		}
		contextPtrs[i] = &contexts[i];
	}

	// One render thread per window for multi-window examples (fork-join); single-window
	// examples render inline on the main thread (no worker).
	std::vector<std::unique_ptr<RenderWorker>> workers;
	int exitCode = 0;
	if (!example.onInit(device, contextPtrs))
	{
		fprintf(stderr, "Example onInit: FAIL\n");
		exitCode = 1;
	}
	else
	{
		printf("%zu window(s) ready — close any to exit\n", viewCount);
		if (viewCount > 1)
			for (uint32_t i = 0; i < viewCount; ++i)
				workers.push_back(std::make_unique<RenderWorker>(&example, i));

		// Optional frame cap for scripted/CI smoke runs (verify N frames then exit cleanly).
		const char* capEnv = std::getenv("TESTBED_FRAME_CAP");
		int frameCap = capEnv ? std::atoi(capEnv) : -1;

		float time = 0.0f;
		while (!anyWindowClosing(windows))
		{
			if (frameCap >= 0 && --frameCap < 0)
				break;
			glfwPollEvents();

			example.onUpdate(device, time);

			if (workers.empty())
			{
				example.onRenderView(0, time); // single window, inline
			}
			else
			{
				for (auto& w : workers)
					w->kick(time);
				for (auto& w : workers)
					w->wait();
			}

			time += 0.016f; // fixed step keeps animation clock-free
		}
	}

	// Teardown, device-before-surface ordered: stop the threads, release the example's
	// resources, the contexts' (swapchain/renderer) handles, then destroy the device
	// (flushing the deferred destroys while the surfaces are still alive), then the
	// surfaces, then the GLFW windows.
	workers.clear(); // joins each thread
	example.onShutdown();
	for (RenderContext& c : contexts)
		c.shutdown();
	device.reset();
	surfaces.clear();
	for (GLFWwindow* w : windows)
		if (w)
			glfwDestroyWindow(w);
	glfwTerminate();
	return exitCode;
}
