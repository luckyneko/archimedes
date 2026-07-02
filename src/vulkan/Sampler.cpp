/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/Sampler.h"

#include "archimedes/vulkan/Device.h"

#include <algorithm>
#include <utility>

acm::vulkan::Sampler::Sampler(acm::vulkan::Device& owner, float maxAnisotropy)
{
	m_owner = &owner;
	const bool anisotropic = maxAnisotropy > 1.0f && owner.enabledFeatures().samplerAnisotropy;
	float anisotropy = 1.0f;
	if (anisotropic)
		anisotropy = std::min(maxAnisotropy, owner.properties().limits.maxSamplerAnisotropy);

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = anisotropic ? VK_TRUE : VK_FALSE;
	samplerInfo.maxAnisotropy = anisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	if (vkCreateSampler(owner.vkDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
		m_error = acm::Error("failed to create sampler");
}

acm::vulkan::Sampler::~Sampler()
{
	release();
}

acm::vulkan::Sampler::Sampler(Sampler&& other) noexcept
{
	*this = std::move(other);
}

acm::vulkan::Sampler& acm::vulkan::Sampler::operator=(Sampler&& other) noexcept
{
	if (this == &other)
		return *this;
	release();
	m_owner = std::exchange(other.m_owner, nullptr);
	m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
	m_error = std::move(other.m_error);
	return *this;
}

VkSampler acm::vulkan::Sampler::vkSampler() const
{
	return m_sampler;
}

void acm::vulkan::Sampler::release()
{
	acm::vulkan::Device* owner = std::exchange(m_owner, nullptr);
	const VkSampler sampler = std::exchange(m_sampler, VK_NULL_HANDLE);
	if (!owner || !sampler)
		return;
	vkDestroySampler(owner->vkDevice(), sampler, nullptr);
}
