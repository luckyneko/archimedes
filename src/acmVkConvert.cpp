#include "acmVkConvert.h"
#include <spdlog/spdlog.h>

namespace acm
{
	namespace detail
	{
		VkFormat toVk(acm::Format format)
		{
			switch(format)
			{
				case acm::Format::R8G8B8A8_Unorm:           return VK_FORMAT_R8G8B8A8_UNORM;
				case acm::Format::R8G8B8A8_Srgb:            return VK_FORMAT_R8G8B8A8_SRGB;
				case acm::Format::B8G8R8A8_Unorm:           return VK_FORMAT_B8G8R8A8_UNORM;
				case acm::Format::B8G8R8A8_Srgb:            return VK_FORMAT_B8G8R8A8_SRGB;
				case acm::Format::A2B10G10R10_Unorm_Pack32: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
				case acm::Format::R16G16B16A16_Sfloat:      return VK_FORMAT_R16G16B16A16_SFLOAT;
				case acm::Format::D32_Sfloat:               return VK_FORMAT_D32_SFLOAT;
				case acm::Format::D24_Unorm_S8_Uint:        return VK_FORMAT_D24_UNORM_S8_UINT;
				case acm::Format::Undefined:                return VK_FORMAT_UNDEFINED;
			}
			spdlog::warn("acm::detail::toVk: invalid acm::Format {0}, using Undefined", int(format));
			return VK_FORMAT_UNDEFINED;
		}

		bool tryFromVk(VkFormat format, acm::Format& out)
		{
			switch(format)
			{
				case VK_FORMAT_R8G8B8A8_UNORM:           out = acm::Format::R8G8B8A8_Unorm;           return true;
				case VK_FORMAT_R8G8B8A8_SRGB:            out = acm::Format::R8G8B8A8_Srgb;            return true;
				case VK_FORMAT_B8G8R8A8_UNORM:           out = acm::Format::B8G8R8A8_Unorm;           return true;
				case VK_FORMAT_B8G8R8A8_SRGB:            out = acm::Format::B8G8R8A8_Srgb;            return true;
				case VK_FORMAT_A2B10G10R10_UNORM_PACK32: out = acm::Format::A2B10G10R10_Unorm_Pack32; return true;
				case VK_FORMAT_R16G16B16A16_SFLOAT:      out = acm::Format::R16G16B16A16_Sfloat;      return true;
				case VK_FORMAT_D32_SFLOAT:               out = acm::Format::D32_Sfloat;               return true;
				case VK_FORMAT_D24_UNORM_S8_UINT:        out = acm::Format::D24_Unorm_S8_Uint;        return true;
				case VK_FORMAT_UNDEFINED:                out = acm::Format::Undefined;                return true;
				default:                                                                              return false;
			}
		}

		VkColorSpaceKHR toVk(acm::ColorSpace colorSpace)
		{
			switch(colorSpace)
			{
				case acm::ColorSpace::SrgbNonlinear: return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			}
			spdlog::warn("acm::detail::toVk: invalid acm::ColorSpace {0}, using SrgbNonlinear", int(colorSpace));
			return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		}

		bool tryFromVk(VkColorSpaceKHR colorSpace, acm::ColorSpace& out)
		{
			switch(colorSpace)
			{
				case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: out = acm::ColorSpace::SrgbNonlinear; return true;
				default:                                                                     return false;
			}
		}

		VkPresentModeKHR toVk(acm::PresentMode mode)
		{
			switch(mode)
			{
				case acm::PresentMode::Immediate:   return VK_PRESENT_MODE_IMMEDIATE_KHR;
				case acm::PresentMode::Mailbox:     return VK_PRESENT_MODE_MAILBOX_KHR;
				case acm::PresentMode::Fifo:        return VK_PRESENT_MODE_FIFO_KHR;
				case acm::PresentMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			}
			spdlog::warn("acm::detail::toVk: invalid acm::PresentMode {0}, using Fifo", int(mode));
			return VK_PRESENT_MODE_FIFO_KHR;
		}

		bool tryFromVk(VkPresentModeKHR mode, acm::PresentMode& out)
		{
			switch(mode)
			{
				case VK_PRESENT_MODE_IMMEDIATE_KHR:    out = acm::PresentMode::Immediate;   return true;
				case VK_PRESENT_MODE_MAILBOX_KHR:      out = acm::PresentMode::Mailbox;     return true;
				case VK_PRESENT_MODE_FIFO_KHR:         out = acm::PresentMode::Fifo;        return true;
				case VK_PRESENT_MODE_FIFO_RELAXED_KHR: out = acm::PresentMode::FifoRelaxed; return true;
				default:                                                                    return false;
			}
		}

		acm::PhysicalDeviceType fromVk(VkPhysicalDeviceType type)
		{
			switch(type)
			{
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return acm::PhysicalDeviceType::IntegratedGpu;
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return acm::PhysicalDeviceType::DiscreteGpu;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return acm::PhysicalDeviceType::VirtualGpu;
				case VK_PHYSICAL_DEVICE_TYPE_CPU:            return acm::PhysicalDeviceType::Cpu;
				case VK_PHYSICAL_DEVICE_TYPE_OTHER:          return acm::PhysicalDeviceType::Other;
				default:                                     return acm::PhysicalDeviceType::Other;
			}
		}

		VkImageType toVk(acm::ImageType type)
		{
			switch(type)
			{
				case acm::ImageType::e1D: return VK_IMAGE_TYPE_1D;
				case acm::ImageType::e2D: return VK_IMAGE_TYPE_2D;
				case acm::ImageType::e3D: return VK_IMAGE_TYPE_3D;
			}
			return VK_IMAGE_TYPE_2D;
		}

		acm::ImageType fromVk(VkImageType type)
		{
			switch(type)
			{
				case VK_IMAGE_TYPE_1D: return acm::ImageType::e1D;
				case VK_IMAGE_TYPE_2D: return acm::ImageType::e2D;
				case VK_IMAGE_TYPE_3D: return acm::ImageType::e3D;
				default:               return acm::ImageType::e2D;
			}
		}

		VkImageViewType toVkImageViewType(acm::ImageType type)
		{
			switch(type)
			{
				case acm::ImageType::e1D: return VK_IMAGE_VIEW_TYPE_1D;
				case acm::ImageType::e2D: return VK_IMAGE_VIEW_TYPE_2D;
				case acm::ImageType::e3D: return VK_IMAGE_VIEW_TYPE_3D;
			}
			return VK_IMAGE_VIEW_TYPE_2D;
		}
	}
}
