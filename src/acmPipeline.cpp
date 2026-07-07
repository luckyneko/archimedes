/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmPipeline.h"

#include "archimedes/backendAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// PipelineConfig
	// -----------------------------------------------------------------------------

	PipelineConfig PipelineConfig::Default()
	{
		return {};
	}

	PipelineConfig PipelineConfig::Mesh3D()
	{
		PipelineConfig config;
		config.cullMode = acm::CullMode::Back;
		config.frontFace = acm::FrontFace::Clockwise;
		return config;
	}

	PipelineConfig PipelineConfig::Sprite2D()
	{
		PipelineConfig config;
		config.topology = acm::Topology::TriangleStrip;
		config.cullMode = acm::CullMode::None;
		config.blend = acm::BlendMode::AlphaBlend;
		return config;
	}

	PipelineConfig PipelineConfig::Wireframe(float lineWidth)
	{
		PipelineConfig config;
		config.cullMode = acm::CullMode::None;
		config.polygonMode = acm::PolygonMode::Line;
		config.lineWidth = lineWidth;
		return config;
	}

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Pipeline::Pipeline() = default;

	Pipeline::Pipeline(const Pipeline& other) = default;

	Pipeline& Pipeline::operator=(const Pipeline& other) = default;

	Pipeline::Pipeline(Pipeline&& other) noexcept = default;

	Pipeline& Pipeline::operator=(Pipeline&& other) noexcept = default;

	Pipeline::~Pipeline() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Pipeline::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool Pipeline::valid() const
	{
		return m_resource.valid();
	}

	Error Pipeline::error() const
	{
		return m_error;
	}

	backend::Pipeline* Pipeline::backend() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	Pipeline::Pipeline(ResourceRef<backend::Pipeline> resource)
		: m_resource(std::move(resource))
	{
	}

	Pipeline::Pipeline(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
