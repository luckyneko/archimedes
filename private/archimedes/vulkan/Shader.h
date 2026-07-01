#pragma once

#include "archimedes/acmError.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Device;

	class Shader
	{
	public:
		Shader() = default;
		Shader(acm::vulkan::Device& owner, const std::vector<char>& spirv);
		~Shader();
		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;
		Shader(Shader&& other) noexcept;
		Shader& operator=(Shader&& other) noexcept;

		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_shaderModule != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkShaderModule vkShaderModule() const;

	private:
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		VkShaderModule m_shaderModule{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
