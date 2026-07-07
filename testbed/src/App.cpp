/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "App.h"

#include "Example.h"
#include "RenderContext.h"
#include "RenderWorker.h"

#include <archimedes/acmVulkanInterop.h>
#include <archimedes/archimedes.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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

		VkSurfaceKHR vulkanSurface{};
		if (glfwCreateWindowSurface(acm::interop::instance(instance), window, nullptr, &vulkanSurface) != VK_SUCCESS)
		{
			fprintf(stderr, "glfwCreateWindowSurface failed\n");
			glfwDestroyWindow(window);
			return {};
		}
		outWindow = window;
		return acm::interop::adoptSurface(instance, vulkanSurface);
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
	acm::useStagedVulkanRuntime();

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
	const std::vector<acm::SurfaceOption> surfaceOptions = instance.surfaceOptions(surfaces);
	if (surfaceOptions.empty())
	{
		fprintf(stderr, "no device presents to all windows\n");
		return 1;
	}
	const acm::SurfaceOption surfaceOption = surfaceOptions.front();
	acm::Device device = instance.createDevice(surfaceOption.device);
	if (!device.valid())
	{
		fprintf(stderr, "CreateACMDevice: FAIL\n");
		return 1;
	}
	printf("Selected device: %s\n", instance.devices()[surfaceOption.device.deviceIndex].name.c_str());

	// A RenderContext per window. Sized up front so the pointers handed to the example
	// (and the workers) stay stable.
	std::vector<RenderContext> contexts(viewCount);
	std::vector<RenderContext*> contextPtrs(viewCount, nullptr);
	for (size_t i = 0; i < viewCount; ++i)
	{
		int fbW = 0, fbH = 0;
		glfwGetFramebufferSize(windows[i], &fbW, &fbH);
		if (!contexts[i].init(device, surfaces[i], surfaceOption, cfg.depth, cfg.samples, acm::Extent2D{uint32_t(fbW), uint32_t(fbH)}))
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
