#include "archimedes/vulkan/Sampler.h"

#include "archimedes/vulkan/Device.h"

#include <algorithm>

bool acm::vulkan::Sampler::create(acm::vulkan::Device& owner, float maxAnisotropy)
{
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
	return vkCreateSampler(owner.vkDevice(), &samplerInfo, nullptr, &m_sampler) == VK_SUCCESS;
}

VkSampler acm::vulkan::Sampler::vkSampler(const acm::Handle& handle) const
{
	return accessible(handle) ? m_sampler : VK_NULL_HANDLE;
}

void acm::vulkan::Sampler::retire(acm::vulkan::Device& owner)
{
	const VkSampler retiredSampler = std::exchange(m_sampler, VK_NULL_HANDLE);
	if (!retiredSampler)
		return;
	const VkDevice device = owner.vkDevice();
	owner.enqueueDestroy([device, retiredSampler]
						 { vkDestroySampler(device, retiredSampler, nullptr); });
}
