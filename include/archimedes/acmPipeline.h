/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmBackend.h"
#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmResourceRef.h"
#include "archimedes/acmShader.h"
#include "archimedes/acmTypes.h"

namespace acm
{
	struct PipelineShaders
	{
		acm::Shader vertex;
		acm::Shader fragment;
		acm::Shader geometry;
	};

	// Optional graphics pipeline state. `vertexLayout` defaults to empty (no vertex
	// input -- geometry from the shader); `descriptorLayout` defaults to null (empty
	// pipeline layout). The target supplies the compatible attachment formats and
	// sample count.
	struct PipelineConfig
	{
		// Presets. Default() is exactly the default-constructed state; the others are
		// opt-in starting points that callers can still customize field-by-field.
		static PipelineConfig Default();
		static PipelineConfig Mesh3D();
		static PipelineConfig Sprite2D();
		static PipelineConfig Wireframe(float lineWidth = 1.0f);

		acm::VertexLayout vertexLayout;
		acm::DescriptorSetLayout descriptorLayout;
		// Must match the target: enable depth test/write only for a target that has a
		// depth attachment.
		acm::DepthState depth;
		acm::Topology topology{acm::Topology::TriangleList};
		acm::CullMode cullMode{acm::CullMode::None};
		acm::FrontFace frontFace{acm::FrontFace::Clockwise};
		acm::BlendMode blend{acm::BlendMode::Opaque};
		// Wireframe (`Line`) needs the device's fillModeNonSolid feature; `lineWidth` > 1
		// needs wideLines. Without the feature the pipeline falls back (Fill / width 1).
		acm::PolygonMode polygonMode{acm::PolygonMode::Fill};
		float lineWidth{1.0f};
		// Per-sample shading fraction (0 = off; 1 = shade every sample). > 0 needs the
		// sampleRateShading feature and a multisampled target; ignored otherwise.
		float minSampleShading{0.0f};
	};

	class Pipeline
	{
	public:
		// Lifetime
		Pipeline();
		Pipeline(const acm::Pipeline& other);
		Pipeline& operator=(const acm::Pipeline& other);
		Pipeline(acm::Pipeline&& other) noexcept;
		Pipeline& operator=(acm::Pipeline&& other) noexcept;
		~Pipeline();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::backend::Pipeline* backend() const;

	private:
		// Construction
		friend acm::backend::Device;
		Pipeline(acm::ResourceRef<acm::backend::Pipeline> resource);
		explicit Pipeline(acm::Error error);

		acm::ResourceRef<acm::backend::Pipeline> m_resource;
		acm::Error m_error;
	};
} // namespace acm
