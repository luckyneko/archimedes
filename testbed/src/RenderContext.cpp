/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "RenderContext.h"

bool RenderContext::init(acm::Device& device, const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, bool depth, acm::SampleCount samples, acm::Extent2D extent)
{
	m_swapChain = device.createSwapChain(surface, format, presentMode, extent, depth, samples);
	if (!m_swapChain.valid())
		return false;
	m_renderer = device.createRenderer(m_swapChain);
	return m_renderer.valid();
}

void RenderContext::shutdown()
{
	// Reset the device-derived handles; their teardown defers onto the device's queue,
	// flushed when the device is destroyed (the App destroys the device before the surfaces).
	m_renderer.reset();
	m_swapChain.reset();
}
