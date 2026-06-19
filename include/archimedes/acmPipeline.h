#pragma once

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmShader.h"
#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"

namespace acm
{
	// Everything a graphics pipeline is built from. `vertexLayout` defaults to empty
	// (no vertex input — geometry from the shader); `descriptorLayout` defaults to
	// null (empty pipeline layout). The fixed-function knobs default to the original
	// smoke-test state (triangle list, no cull, clockwise, opaque), so existing
	// callers are unchanged. A config struct rather than a pile of createPipeline
	// overloads, since these are independent optional knobs.
	struct PipelineConfig
	{
		acm::Shader vertex;
		acm::Shader fragment;
		VkRenderPass renderPass{};
		acm::VertexLayout vertexLayout;
		acm::DescriptorSetLayout descriptorLayout;
		// Enable depth test + write (compare LESS). Must match the render pass: set it
		// only for a pass that has a depth attachment (a depth-enabled RenderTarget).
		bool depthTest{false};
		acm::Topology topology{acm::Topology::TriangleList};
		acm::CullMode cullMode{acm::CullMode::None};
		acm::FrontFace frontFace{acm::FrontFace::Clockwise};
		acm::BlendMode blend{acm::BlendMode::Opaque};
		// Wireframe (`Line`) needs the device's fillModeNonSolid feature; `lineWidth` > 1
		// needs wideLines. Without the feature the pipeline falls back (Fill / width 1)
		// and warns, rather than producing an invalid pipeline.
		acm::PolygonMode polygonMode{acm::PolygonMode::Fill};
		float lineWidth{1.0f};
		// MSAA sample count — must match the render target's (clamped the same way).
		acm::SampleCount samples{acm::SampleCount::One};
		// Per-sample shading fraction (0 = off; 1 = shade every sample). > 0 needs the
		// sampleRateShading feature and samples > 1; ignored / warns otherwise. Smooths
		// aliasing *inside* a primitive (high-frequency shaders), beyond edge MSAA.
		float minSampleShading{0.0f};
	};

	class Pipeline
	{
	public:
		Pipeline() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		VkPipeline vkPipeline() const;
		VkPipelineLayout vkPipelineLayout() const;

	private:
		friend class Device; // only Device::createPipeline builds one
		// A graphics pipeline built from `config`: vertex + fragment stages, the
		// config's vertex input + descriptor set layout, depth + topology/cull/front/
		// blend state, over its render pass. Viewport + scissor are dynamic (set
		// per-frame on the command buffer), so the pipeline is resolution-independent
		// and survives swapchain recreation. Still baked in: FILL polygon mode and no
		// MSAA.
		Pipeline(acm::Device device, const acm::PipelineConfig& config);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
