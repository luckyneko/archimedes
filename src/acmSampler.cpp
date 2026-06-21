#include "archimedes/acmSampler.h"

#include "archimedes/acmDevice.h"

#include <vulkan/vulkan.h>

#include <algorithm>

struct acm::Sampler::impl
{
	acm::Device device;
	VkSampler sampler{VK_NULL_HANDLE};

	~impl()
	{
		if (sampler && device.valid())
		{
			VkDevice dev = device.vkDevice();
			VkSampler s = sampler;
			device.enqueueDestroy([dev, s]
								  { vkDestroySampler(dev, s, nullptr); });
		}
	}
};

acm::Sampler::Sampler(acm::Device device, float maxAnisotropy)
	: m()
{
	auto impl = std::make_shared<acm::Sampler::impl>();
	impl->device = device;

	// Anisotropy is requested with maxAnisotropy > 1. It needs the samplerAnisotropy
	// device feature; without it we disable (silently degrade) rather than make an
	// invalid sampler. The level is clamped to the device's maxSamplerAnisotropy limit.
	bool aniso = maxAnisotropy > 1.0f;
	if (aniso && !device.enabledFeatures().samplerAnisotropy)
		aniso = false;
	float anisoLevel = 1.0f;
	if (aniso)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(device.getGPU().device, &props);
		anisoLevel = std::min(maxAnisotropy, props.limits.maxSamplerAnisotropy);
	}

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	// Trilinear + the full LOD range, so a mipmapped texture is filtered across its
	// levels. Harmless for single-level textures (clamps to level 0).
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = aniso ? VK_TRUE : VK_FALSE;
	samplerInfo.maxAnisotropy = anisoLevel;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	if (vkCreateSampler(impl->device.vkDevice(), &samplerInfo, nullptr, &impl->sampler) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create sampler");
		return;
	}

	m = impl;
}

VkSampler acm::Sampler::vkSampler() const
{
	return m->sampler;
}
