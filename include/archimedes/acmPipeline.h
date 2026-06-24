#pragma once

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmNative.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmShader.h"
#include "archimedes/acmTypes.h"

namespace acm
{
	// Everything a graphics pipeline is built from. `vertexLayout` defaults to empty
	// (no vertex input -- geometry from the shader); `descriptorLayout` defaults to
	// null (empty pipeline layout). The target supplies the compatible render pass.
	struct PipelineConfig
	{
		acm::Shader vertex;
		acm::Shader fragment;
		acm::RenderTarget target;
		acm::VertexLayout vertexLayout;
		acm::DescriptorSetLayout descriptorLayout;
		// Enable depth test + write (compare LESS). Must match the target: set it only
		// for a target that has a depth attachment.
		bool depthTest{false};
		acm::Topology topology{acm::Topology::TriangleList};
		acm::CullMode cullMode{acm::CullMode::None};
		acm::FrontFace frontFace{acm::FrontFace::Clockwise};
		acm::BlendMode blend{acm::BlendMode::Opaque};
		// Wireframe (`Line`) needs the device's fillModeNonSolid feature; `lineWidth` > 1
		// needs wideLines. Without the feature the pipeline falls back (Fill / width 1).
		acm::PolygonMode polygonMode{acm::PolygonMode::Fill};
		float lineWidth{1.0f};
		// MSAA sample count -- must match the render target's (clamped the same way).
		acm::SampleCount samples{acm::SampleCount::One};
		// Per-sample shading fraction (0 = off; 1 = shade every sample). > 0 needs the
		// sampleRateShading feature and samples > 1; ignored otherwise.
		float minSampleShading{0.0f};
	};

	class Pipeline
	{
	public:
		Pipeline();
		Pipeline(const acm::Pipeline& other);
		Pipeline& operator=(const acm::Pipeline& other);
		Pipeline(acm::Pipeline&& other) noexcept;
		Pipeline& operator=(acm::Pipeline&& other) noexcept;
		~Pipeline();

		void reset();
		bool valid() const;
		acm::Error error() const;
		const acm::Handle& handle() const { return m_handle; }
		acm::native::Pipeline* native() const { return m_resource; }

	private:
		friend acm::native::Device;
		Pipeline(acm::native::Pipeline* resource, acm::Handle handle);
		explicit Pipeline(acm::Error error);

		acm::native::Pipeline* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
