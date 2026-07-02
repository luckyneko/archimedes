/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"
#include "archimedes/acmNative.h"

#include <cstddef>
#include <cstdint>

namespace acm
{
	// A descriptor set allocated against a DescriptorSetLayout, plus the (small,
	// single-set) pool it lives in — folded together so callers don't juggle a
	// separate pool. Point a binding at a texture+sampler with setTexture() or a
	// buffer with setBuffer(), then bind it for a draw via
	// CommandBuffer::bindDescriptorSet. `arrayElement` targets an element of a
	// descriptor-array binding (`DescriptorBinding::count` > 1); 0 for a plain binding.
	class DescriptorSet
	{
	public:
		DescriptorSet();
		DescriptorSet(const acm::DescriptorSet& other);
		DescriptorSet& operator=(const acm::DescriptorSet& other);
		DescriptorSet(acm::DescriptorSet&& other) noexcept;
		DescriptorSet& operator=(acm::DescriptorSet&& other) noexcept;
		~DescriptorSet();

		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::native::DescriptorSet* native() const;

		// Writes a combined image sampler at `binding` (the texture is sampled in
		// SHADER_READ_ONLY layout — i.e. it was rendered/uploaded ready to sample).
		void setTexture(uint32_t binding, const acm::Texture& texture, const acm::Sampler& sampler, uint32_t arrayElement = 0);

		// Writes a buffer at `binding` — a uniform or storage descriptor, matching the
		// layout binding's type. The buffer is host-visible (write it with Buffer::write).
		void setBuffer(uint32_t binding, const acm::Buffer& buffer, uint32_t arrayElement = 0);

		// Writes a dynamic uniform buffer at `binding` (layout type UniformBufferDynamic):
		// the descriptor covers `elementSize` bytes and the per-draw base offset is given
		// at bind time via CommandBuffer::bindDescriptorSet(..., dynamicOffset). Lets one
		// buffer hold many objects' constants. `elementSize` is the size one draw reads.
		void setDynamicBuffer(uint32_t binding, const acm::Buffer& buffer, size_t elementSize, uint32_t arrayElement = 0);

		// Writes a storage image at `binding` (layout type StorageImage) — a shader-
		// writable image with no sampler, bound in GENERAL layout. The texture must have
		// been created with `storage`; transition it to General before the shader writes it.
		void setStorageImage(uint32_t binding, const acm::Texture& texture, uint32_t arrayElement = 0);

	private:
		friend acm::native::Device;
		DescriptorSet(acm::ResourceRef<acm::native::DescriptorSet> resource);
		explicit DescriptorSet(acm::Error error);

		acm::ResourceRef<acm::native::DescriptorSet> m_resource;
		acm::Error m_error;
	};
} // namespace acm
