#pragma once

#include "Example.h"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

// Drives one window of a multi-window example on its own render thread, in a fork-join
// with the App loop: after the main thread runs the example's onUpdate, it kick()s every
// worker, which call example->onRenderView(viewIndex, time) concurrently; the App then
// wait()s for all before the next update. Workers do only Vulkan (no GLFW), so they are
// safe off the main thread; cross-thread queue access is serialized inside acm::Renderer
// (acm::Device::deviceMutex). Single-window examples skip workers and render inline.
class RenderWorker
{
public:
	RenderWorker(Example* example, uint32_t viewIndex)
		: m_example(example)
		, m_viewIndex(viewIndex)
	{
		m_thread = std::thread([this]
							   { loop(); });
	}

	~RenderWorker() { stop(); }

	RenderWorker(const RenderWorker&) = delete;
	RenderWorker& operator=(const RenderWorker&) = delete;

	// Main thread: release the worker to render one frame at `time`.
	void kick(float time)
	{
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			m_time = time;
			m_go = true;
			m_done = false;
		}
		m_cv.notify_one();
	}

	// Main thread: block until the kicked frame has been recorded + submitted.
	void wait()
	{
		std::unique_lock<std::mutex> lock(m_mtx);
		m_cv.wait(lock, [this]
				  { return m_done; });
	}

	// Main thread: signal the worker to exit and join it. Idempotent.
	void stop()
	{
		if (!m_thread.joinable())
			return;
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			m_quit = true;
			m_go = true;
		}
		m_cv.notify_one();
		m_thread.join();
	}

private:
	void loop()
	{
		for (;;)
		{
			float time = 0.0f;
			{
				std::unique_lock<std::mutex> lock(m_mtx);
				m_cv.wait(lock, [this]
						  { return m_go; });
				m_go = false;
				if (m_quit)
					return;
				time = m_time;
			}

			m_example->onRenderView(m_viewIndex, time);

			{
				std::lock_guard<std::mutex> lock(m_mtx);
				m_done = true;
			}
			m_cv.notify_one();
		}
	}

	Example* m_example;
	uint32_t m_viewIndex;
	std::thread m_thread;
	std::mutex m_mtx;
	std::condition_variable m_cv;
	float m_time{0.0f};
	bool m_go{false};
	bool m_done{true};
	bool m_quit{false};
};
