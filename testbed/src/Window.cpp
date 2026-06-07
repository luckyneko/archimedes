#include "Window.h"

#include <archimedes/archimedes.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

struct Window::impl
{
	WindowDelegatePtr delegate;
	acm::Instance instance;
	GLFWwindow* window = nullptr;
	acm::Surface surface;
	acm::Device device;
	acm::SwapChain swapChain;

	~impl()
	{
		if(delegate)
			delegate->onShutdown(device, swapChain);

		if(window)
			glfwDestroyWindow(window);
		glfwTerminate();
	}
};

Window::Window(WindowDelegatePtr delegate, acm::Instance instance, const std::string& name, int width, int height)
{
	bool ok = true;
	auto impl = std::make_shared<Window::impl>();
	impl->delegate = delegate;
	impl->instance = instance;

	// Route GLFW through the vendored loader we link against, rather than a
	// dlopen'd system libvulkan. Must precede glfwInit.
	glfwInitVulkanLoader(reinterpret_cast<PFN_vkGetInstanceProcAddr>(vkGetInstanceProcAddr));

	// GLFW init
	if(ok)
	{
		ok = (glfwInit() == GLFW_TRUE);
		if(!ok)
			spdlog::error("glfwInit failed");
		spdlog::info("InitGLFW: {0}", ok ? "ok" : "FAIL");
	}

	// GLFW window (no OpenGL context — Vulkan only)
	if(ok)
	{
		ok = (glfwVulkanSupported() == GLFW_TRUE);
		if(!ok)
			spdlog::error("glfwVulkanSupported: no Vulkan loader/ICD found");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		// No swapchain-recreation path yet, so keep the surface size fixed.
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		impl->window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
		ok = ok && (impl->window != nullptr);
		if(!ok)
			spdlog::error("glfwCreateWindow failed");
		spdlog::info("CreateGLFWWindow: {0}", ok ? "ok" : "FAIL");
	}

	// ACM Surface
	if(ok)
	{
		VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
		ok = (glfwCreateWindowSurface(impl->instance.vkInstance(), impl->window, nullptr, &vkSurface) == VK_SUCCESS);
		if(!ok)
			spdlog::error("glfwCreateWindowSurface failed");
		spdlog::info("CreateGLFWSurface: {0}", ok ? "ok" : "FAIL");

		if(ok)
		{
			impl->surface = acm::Surface(impl->instance, vkSurface);
			ok = impl->surface.valid();
			spdlog::info("CreateACMSurface: {0}", ok ? "ok" : "FAIL");
		}
	}

	// SwapChain settings (delegate picks GPU/queue/format/present mode)
	SwapChainSettings settings;
	if(ok)
	{
		settings = impl->delegate->onSelectSwapChainSettings(impl->instance.getAvailableGPUs(), impl->surface.getGPUSupport());
		const auto& selectedGPU = impl->instance.getAvailableGPUs()[settings.selectedGPUIdx];
		spdlog::info("SelectedGPU: {0}", selectedGPU.properties.deviceName);
		spdlog::info("SelectedQueueFamily: {0}", settings.selectedQueueFamilyIdx);
		spdlog::info("SelectedFormat: {0}", int(settings.selectedFormat.format));
		spdlog::info("SelectedColourSpace: {0}", int(settings.selectedFormat.colorSpace));
		spdlog::info("SelectedPresentMode: {0}", int(settings.selectedPresentMode));
	}

	// ACM Device
	if(ok)
	{
		const auto& selectedGPU = impl->instance.getAvailableGPUs()[settings.selectedGPUIdx];
		impl->device = acm::Device(impl->instance, selectedGPU, settings.selectedQueueFamilyIdx);
		ok = impl->device.valid();
		spdlog::info("CreateACMDevice: {0}", ok ? "ok" : "FAIL");
	}

	// ACM SwapChain
	if(ok)
	{
		impl->swapChain = acm::SwapChain(impl->device, impl->surface, settings.selectedFormat, settings.selectedPresentMode);
		ok = impl->swapChain.valid();
		spdlog::info("CreateACMSwapChain: {0}", ok ? "ok" : "FAIL");
	}

	if(ok)
		m = std::move(impl);

	if(m && m->delegate)
		m->delegate->onInit(m->device, m->swapChain);
}

void Window::update()
{
	if(!m)
		return;

	glfwPollEvents();
	if(glfwWindowShouldClose(m->window))
	{
		reset();
		return;
	}

	if(m->delegate)
		m->delegate->onUpdate();
}

void Window::render()
{
	if(m && m->delegate)
		m->delegate->onRender(m->device, m->swapChain);
}
