/*
 *  Created by LuckyNeko on 16/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <cstdint>
#include <vector>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Geometry
	// -----------------------------------------------------------------------------

	struct Extent2D
	{
		uint32_t width{0};
		uint32_t height{0};
	};

	// -----------------------------------------------------------------------------
	// Formats And Surfaces
	// -----------------------------------------------------------------------------

	enum class ColorSpace
	{
		SrgbNonlinear,
	};

	// Backend-neutral image / vertex-attribute formats. This deliberately curated
	// set covers the surface, image, and vertex formats used by the renderer today.
	// Extend the enum and the selected backend's conversion table together.
	enum class Format
	{
		Undefined,
		R8G8B8A8_Unorm,
		R8G8B8A8_Srgb,
		B8G8R8A8_Unorm,
		B8G8R8A8_Srgb,
		A2B10G10R10_Unorm_Pack32,
		R16G16B16A16_Sfloat,
		D32_Sfloat,
		D24_Unorm_S8_Uint,
		R32_Sfloat,
		R32G32_Sfloat,
		R32G32B32_Sfloat,
		R32G32B32A32_Sfloat,
	};

	enum class PresentMode
	{
		Immediate,
		Mailbox,
		Fifo,
		FifoRelaxed,
	};

	// Backend-neutral surface capabilities. Swapchain creation requeries the
	// complete native capabilities through the private backend.
	struct SurfaceCapabilities
	{
		uint32_t minImageCount{0};
		uint32_t maxImageCount{0};
		Extent2D currentExtent;
		Extent2D minImageExtent;
		Extent2D maxImageExtent;
	};

	struct SurfaceFormat
	{
		Format format{Format::Undefined};
		ColorSpace colorSpace{ColorSpace::SrgbNonlinear};
	};

	// -----------------------------------------------------------------------------
	// Device
	// -----------------------------------------------------------------------------

	enum class PhysicalDeviceType
	{
		Other,
		IntegratedGpu,
		DiscreteGpu,
		VirtualGpu,
		Cpu,
	};

	// Multisample anti-aliasing sample count. A request is clamped to what the device
	// supports (so `Eight` may resolve to fewer). `One` is plain, single-sampled.
	enum class SampleCount
	{
		One,
		Two,
		Four,
		Eight,
	};

	// -----------------------------------------------------------------------------
	// Pipeline State
	// -----------------------------------------------------------------------------

	// Fixed-function pipeline state knobs (see acm::PipelineConfig). Defaults preserve
	// the original smoke-test behavior: triangle list, no culling, clockwise front
	// face, opaque (no blend).

	// Color blending. `Opaque` overwrites; `AlphaBlend` is standard src-alpha "over".
	enum class BlendMode
	{
		Opaque,
		AlphaBlend,
	};

	// Depth comparison used by DepthState::Test / TestWrite.
	enum class CompareOp
	{
		Never,
		Less,
		Equal,
		LessOrEqual,
		Greater,
		NotEqual,
		GreaterOrEqual,
		Always,
	};

	// Which face (if any) the rasterizer discards. `Back` with the right `FrontFace` is
	// the usual 3D default; `None` draws both sides (geometry can't hide via winding).
	enum class CullMode
	{
		None,
		Back,
		Front,
	};

	// Depth testing/writing state. Use the named constructors below so call sites
	// carry intent instead of adjacent bools. Depth write without depth test is invalid.
	struct DepthState
	{
		static DepthState None() { return {}; }
		static DepthState Test(acm::CompareOp compare = acm::CompareOp::Less) { return {true, false, compare}; }
		static DepthState TestWrite(acm::CompareOp compare = acm::CompareOp::Less) { return {true, true, compare}; }

		bool test{false};
		bool write{false};
		acm::CompareOp compare{acm::CompareOp::Less};
	};

	// Which winding (in framebuffer space) counts as the front face.
	enum class FrontFace
	{
		Clockwise,
		CounterClockwise,
	};

	// How polygons are rasterized. `Line` (wireframe) needs the device's
	// `fillModeNonSolid` feature — a pipeline falls back to `Fill` without it.
	enum class PolygonMode
	{
		Fill,
		Line,
	};

	// What an offscreen RenderTarget leaves its texture ready for once the render
	// rendering ends — drives the color attachment's final layout, so no manual barrier
	// is needed. Sampled => SHADER_READ_ONLY (read it in a later draw); CopySrc =>
	// TRANSFER_SRC (copy it to a buffer / another image).
	enum class RenderTargetFinish
	{
		Sampled,
		CopySrc,
	};

	struct RenderTargetConfig
	{
		RenderTargetFinish finish{RenderTargetFinish::Sampled};
		bool depth{false};
		SampleCount samples{SampleCount::One};
	};

	struct SwapChainConfig
	{
		Extent2D extent;
		bool depth{false};
		SampleCount samples{SampleCount::One};
	};

	// How input vertices/indices assemble into primitives.
	enum class Topology
	{
		TriangleList,
		TriangleStrip,
		LineList,
		LineStrip,
		PointList,
	};

	// One vertex attribute: which shader `location` it feeds, its `format` (e.g.
	// R32G32_Sfloat for a vec2), and its byte `offset` within the vertex struct.
	struct VertexAttribute
	{
		uint32_t location{0};
		Format format{Format::Undefined};
		uint32_t offset{0};
	};

	// Describes the per-vertex data a pipeline reads from a single vertex buffer
	// (binding 0): the `stride` (size of one vertex) and its attributes. An empty
	// layout (stride 0 / no attributes) means no vertex input — geometry comes from
	// the shader (e.g. gl_VertexIndex).
	struct VertexLayout
	{
		uint32_t stride{0};
		std::vector<VertexAttribute> attributes;
	};

	// -----------------------------------------------------------------------------
	// Resources And Descriptors
	// -----------------------------------------------------------------------------

	// What a Buffer is for. Also decides its memory heap: Vertex/Index are device-local
	// (filled via staging), Uniform/TransferDst/Staging are host-visible (CPU-mapped).
	// `Staging` is a host-visible transfer *source* — the scratch buffer that feeds a
	// device-local buffer or a texture upload. One purpose each — no combined usages
	// (e.g. a buffer that is both vertex and a transfer source) yet.
	enum class BufferUsage
	{
		Vertex,
		Index,
		Uniform,
		TransferDst,
		Staging,
		Storage, // host-visible storage buffer — shader read/write, CPU-mappable
	};

	// The kinds of resource a descriptor binding can point at: a uniform buffer
	// (per-draw constants — an MVP matrix, colors), a combined image sampler (a texture
	// + how to sample it), a storage buffer (shader-writable bulk data), or a dynamic
	// uniform buffer (one buffer holding many objects' constants, indexed by a per-draw
	// byte offset supplied at bind time — see DescriptorSet::setDynamicBuffer).
	enum class DescriptorType
	{
		UniformBuffer,
		CombinedImageSampler,
		StorageBuffer,
		UniformBufferDynamic,
		StorageImage, // a shader-writable image (no sampler) — e.g. a compute target
	};

	// A texture layout for CommandBuffer::transitionImage. Covers the cases the renderer
	// transitions by hand — chiefly around a compute storage-image write (Undefined →
	// General to write, General → TransferSrc / ShaderReadOnly to copy out / sample).
	enum class ImageLayout
	{
		Undefined,
		General,		// storage image read/write
		ShaderReadOnly, // sampled in a shader
		TransferSrc,	// copy source (e.g. copyTextureToBuffer)
		TransferDst,	// copy destination
	};

	// Which shader stage(s) a descriptor binding is visible to. A flag set, so values
	// combine: `ShaderStage::Vertex | ShaderStage::Fragment` for a binding both stages
	// read. `Compute` is for bindings a compute pipeline reads/writes (stands alone — it
	// isn't part of the graphics stages).
	enum class ShaderStage : uint32_t
	{
		Vertex = 1u << 0,
		Fragment = 1u << 1,
		Compute = 1u << 2,
	};
	constexpr ShaderStage operator|(ShaderStage a, ShaderStage b)
	{
		return ShaderStage(uint32_t(a) | uint32_t(b));
	}

	// One entry in a DescriptorSetLayout: the `binding` index a shader references
	// (set 0), the resource `type` bound there, the `stage`(s) that read it, and
	// `count` (> 1 makes it a descriptor array, indexed by `arrayElement` when written).
	struct DescriptorBinding
	{
		uint32_t binding{0};
		DescriptorType type{DescriptorType::UniformBuffer};
		ShaderStage stage{ShaderStage::Vertex};
		uint32_t count{1};
	};
} // namespace acm
