#include "vulkan.h"
#include "util.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char* vulkan_get_format_name(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";
		case VK_FORMAT_R4G4_UNORM_PACK8: return "VK_FORMAT_R4G4_UNORM_PACK8";
		case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return "VK_FORMAT_R4G4B4A4_UNORM_PACK16";
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return "VK_FORMAT_B4G4R4A4_UNORM_PACK16";
		case VK_FORMAT_R5G6B5_UNORM_PACK16: return "VK_FORMAT_R5G6B5_UNORM_PACK16";
		case VK_FORMAT_B5G6R5_UNORM_PACK16: return "VK_FORMAT_B5G6R5_UNORM_PACK16";
		case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return "VK_FORMAT_R5G5B5A1_UNORM_PACK16";
		case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return "VK_FORMAT_B5G5R5A1_UNORM_PACK16";
		case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return "VK_FORMAT_A1R5G5B5_UNORM_PACK16";
		case VK_FORMAT_R8_UNORM: return "VK_FORMAT_R8_UNORM";
		case VK_FORMAT_R8_SNORM: return "VK_FORMAT_R8_SNORM";
		case VK_FORMAT_R8_USCALED: return "VK_FORMAT_R8_USCALED";
		case VK_FORMAT_R8_SSCALED: return "VK_FORMAT_R8_SSCALED";
		case VK_FORMAT_R8_UINT: return "VK_FORMAT_R8_UINT";
		case VK_FORMAT_R8_SINT: return "VK_FORMAT_R8_SINT";
		case VK_FORMAT_R8_SRGB: return "VK_FORMAT_R8_SRGB";
		case VK_FORMAT_R8G8_UNORM: return "VK_FORMAT_R8G8_UNORM";
		case VK_FORMAT_R8G8_SNORM: return "VK_FORMAT_R8G8_SNORM";
		case VK_FORMAT_R8G8_USCALED: return "VK_FORMAT_R8G8_USCALED";
		case VK_FORMAT_R8G8_SSCALED: return "VK_FORMAT_R8G8_SSCALED";
		case VK_FORMAT_R8G8_UINT: return "VK_FORMAT_R8G8_UINT";
		case VK_FORMAT_R8G8_SINT: return "VK_FORMAT_R8G8_SINT";
		case VK_FORMAT_R8G8_SRGB: return "VK_FORMAT_R8G8_SRGB";
		case VK_FORMAT_R8G8B8_UNORM: return "VK_FORMAT_R8G8B8_UNORM";
		case VK_FORMAT_R8G8B8_SNORM: return "VK_FORMAT_R8G8B8_SNORM";
		case VK_FORMAT_R8G8B8_USCALED: return "VK_FORMAT_R8G8B8_USCALED";
		case VK_FORMAT_R8G8B8_SSCALED: return "VK_FORMAT_R8G8B8_SSCALED";
		case VK_FORMAT_R8G8B8_UINT: return "VK_FORMAT_R8G8B8_UINT";
		case VK_FORMAT_R8G8B8_SINT: return "VK_FORMAT_R8G8B8_SINT";
		case VK_FORMAT_R8G8B8_SRGB: return "VK_FORMAT_R8G8B8_SRGB";
		case VK_FORMAT_B8G8R8_UNORM: return "VK_FORMAT_B8G8R8_UNORM";
		case VK_FORMAT_B8G8R8_SNORM: return "VK_FORMAT_B8G8R8_SNORM";
		case VK_FORMAT_B8G8R8_USCALED: return "VK_FORMAT_B8G8R8_USCALED";
		case VK_FORMAT_B8G8R8_SSCALED: return "VK_FORMAT_B8G8R8_SSCALED";
		case VK_FORMAT_B8G8R8_UINT: return "VK_FORMAT_B8G8R8_UINT";
		case VK_FORMAT_B8G8R8_SINT: return "VK_FORMAT_B8G8R8_SINT";
		case VK_FORMAT_B8G8R8_SRGB: return "VK_FORMAT_B8G8R8_SRGB";
		case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
		case VK_FORMAT_R8G8B8A8_SNORM: return "VK_FORMAT_R8G8B8A8_SNORM";
		case VK_FORMAT_R8G8B8A8_USCALED: return "VK_FORMAT_R8G8B8A8_USCALED";
		case VK_FORMAT_R8G8B8A8_SSCALED: return "VK_FORMAT_R8G8B8A8_SSCALED";
		case VK_FORMAT_R8G8B8A8_UINT: return "VK_FORMAT_R8G8B8A8_UINT";
		case VK_FORMAT_R8G8B8A8_SINT: return "VK_FORMAT_R8G8B8A8_SINT";
		case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
		case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
		case VK_FORMAT_B8G8R8A8_SNORM: return "VK_FORMAT_B8G8R8A8_SNORM";
		case VK_FORMAT_B8G8R8A8_USCALED: return "VK_FORMAT_B8G8R8A8_USCALED";
		case VK_FORMAT_B8G8R8A8_SSCALED: return "VK_FORMAT_B8G8R8A8_SSCALED";
		case VK_FORMAT_B8G8R8A8_UINT: return "VK_FORMAT_B8G8R8A8_UINT";
		case VK_FORMAT_B8G8R8A8_SINT: return "VK_FORMAT_B8G8R8A8_SINT";
		case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return "VK_FORMAT_A8B8G8R8_UNORM_PACK32";
		case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return "VK_FORMAT_A8B8G8R8_SNORM_PACK32";
		case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return "VK_FORMAT_A8B8G8R8_USCALED_PACK32";
		case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return "VK_FORMAT_A8B8G8R8_SSCALED_PACK32";
		case VK_FORMAT_A8B8G8R8_UINT_PACK32: return "VK_FORMAT_A8B8G8R8_UINT_PACK32";
		case VK_FORMAT_A8B8G8R8_SINT_PACK32: return "VK_FORMAT_A8B8G8R8_SINT_PACK32";
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return "VK_FORMAT_A8B8G8R8_SRGB_PACK32";
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
		case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return "VK_FORMAT_A2R10G10B10_SNORM_PACK32";
		case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return "VK_FORMAT_A2R10G10B10_USCALED_PACK32";
		case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return "VK_FORMAT_A2R10G10B10_SSCALED_PACK32";
		case VK_FORMAT_A2R10G10B10_UINT_PACK32: return "VK_FORMAT_A2R10G10B10_UINT_PACK32";
		case VK_FORMAT_A2R10G10B10_SINT_PACK32: return "VK_FORMAT_A2R10G10B10_SINT_PACK32";
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
		case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return "VK_FORMAT_A2B10G10R10_SNORM_PACK32";
		case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return "VK_FORMAT_A2B10G10R10_USCALED_PACK32";
		case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return "VK_FORMAT_A2B10G10R10_SSCALED_PACK32";
		case VK_FORMAT_A2B10G10R10_UINT_PACK32: return "VK_FORMAT_A2B10G10R10_UINT_PACK32";
		case VK_FORMAT_A2B10G10R10_SINT_PACK32: return "VK_FORMAT_A2B10G10R10_SINT_PACK32";
		case VK_FORMAT_R16_UNORM: return "VK_FORMAT_R16_UNORM";
		case VK_FORMAT_R16_SNORM: return "VK_FORMAT_R16_SNORM";
		case VK_FORMAT_R16_USCALED: return "VK_FORMAT_R16_USCALED";
		case VK_FORMAT_R16_SSCALED: return "VK_FORMAT_R16_SSCALED";
		case VK_FORMAT_R16_UINT: return "VK_FORMAT_R16_UINT";
		case VK_FORMAT_R16_SINT: return "VK_FORMAT_R16_SINT";
		case VK_FORMAT_R16_SFLOAT: return "VK_FORMAT_R16_SFLOAT";
		case VK_FORMAT_R16G16_UNORM: return "VK_FORMAT_R16G16_UNORM";
		case VK_FORMAT_R16G16_SNORM: return "VK_FORMAT_R16G16_SNORM";
		case VK_FORMAT_R16G16_USCALED: return "VK_FORMAT_R16G16_USCALED";
		case VK_FORMAT_R16G16_SSCALED: return "VK_FORMAT_R16G16_SSCALED";
		case VK_FORMAT_R16G16_UINT: return "VK_FORMAT_R16G16_UINT";
		case VK_FORMAT_R16G16_SINT: return "VK_FORMAT_R16G16_SINT";
		case VK_FORMAT_R16G16_SFLOAT: return "VK_FORMAT_R16G16_SFLOAT";
		case VK_FORMAT_R16G16B16_UNORM: return "VK_FORMAT_R16G16B16_UNORM";
		case VK_FORMAT_R16G16B16_SNORM: return "VK_FORMAT_R16G16B16_SNORM";
		case VK_FORMAT_R16G16B16_USCALED: return "VK_FORMAT_R16G16B16_USCALED";
		case VK_FORMAT_R16G16B16_SSCALED: return "VK_FORMAT_R16G16B16_SSCALED";
		case VK_FORMAT_R16G16B16_UINT: return "VK_FORMAT_R16G16B16_UINT";
		case VK_FORMAT_R16G16B16_SINT: return "VK_FORMAT_R16G16B16_SINT";
		case VK_FORMAT_R16G16B16_SFLOAT: return "VK_FORMAT_R16G16B16_SFLOAT";
		case VK_FORMAT_R16G16B16A16_UNORM: return "VK_FORMAT_R16G16B16A16_UNORM";
		case VK_FORMAT_R16G16B16A16_SNORM: return "VK_FORMAT_R16G16B16A16_SNORM";
		case VK_FORMAT_R16G16B16A16_USCALED: return "VK_FORMAT_R16G16B16A16_USCALED";
		case VK_FORMAT_R16G16B16A16_SSCALED: return "VK_FORMAT_R16G16B16A16_SSCALED";
		case VK_FORMAT_R16G16B16A16_UINT: return "VK_FORMAT_R16G16B16A16_UINT";
		case VK_FORMAT_R16G16B16A16_SINT: return "VK_FORMAT_R16G16B16A16_SINT";
		case VK_FORMAT_R16G16B16A16_SFLOAT: return "VK_FORMAT_R16G16B16A16_SFLOAT";
		case VK_FORMAT_R32_UINT: return "VK_FORMAT_R32_UINT";
		case VK_FORMAT_R32_SINT: return "VK_FORMAT_R32_SINT";
		case VK_FORMAT_R32_SFLOAT: return "VK_FORMAT_R32_SFLOAT";
		case VK_FORMAT_R32G32_UINT: return "VK_FORMAT_R32G32_UINT";
		case VK_FORMAT_R32G32_SINT: return "VK_FORMAT_R32G32_SINT";
		case VK_FORMAT_R32G32_SFLOAT: return "VK_FORMAT_R32G32_SFLOAT";
		case VK_FORMAT_R32G32B32_UINT: return "VK_FORMAT_R32G32B32_UINT";
		case VK_FORMAT_R32G32B32_SINT: return "VK_FORMAT_R32G32B32_SINT";
		case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
		case VK_FORMAT_R32G32B32A32_UINT: return "VK_FORMAT_R32G32B32A32_UINT";
		case VK_FORMAT_R32G32B32A32_SINT: return "VK_FORMAT_R32G32B32A32_SINT";
		case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
		case VK_FORMAT_R64_UINT: return "VK_FORMAT_R64_UINT";
		case VK_FORMAT_R64_SINT: return "VK_FORMAT_R64_SINT";
		case VK_FORMAT_R64_SFLOAT: return "VK_FORMAT_R64_SFLOAT";
		case VK_FORMAT_R64G64_UINT: return "VK_FORMAT_R64G64_UINT";
		case VK_FORMAT_R64G64_SINT: return "VK_FORMAT_R64G64_SINT";
		case VK_FORMAT_R64G64_SFLOAT: return "VK_FORMAT_R64G64_SFLOAT";
		case VK_FORMAT_R64G64B64_UINT: return "VK_FORMAT_R64G64B64_UINT";
		case VK_FORMAT_R64G64B64_SINT: return "VK_FORMAT_R64G64B64_SINT";
		case VK_FORMAT_R64G64B64_SFLOAT: return "VK_FORMAT_R64G64B64_SFLOAT";
		case VK_FORMAT_R64G64B64A64_UINT: return "VK_FORMAT_R64G64B64A64_UINT";
		case VK_FORMAT_R64G64B64A64_SINT: return "VK_FORMAT_R64G64B64A64_SINT";
		case VK_FORMAT_R64G64B64A64_SFLOAT: return "VK_FORMAT_R64G64B64A64_SFLOAT";
		case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return "VK_FORMAT_B10G11R11_UFLOAT_PACK32";
		case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32";
		case VK_FORMAT_D16_UNORM: return "VK_FORMAT_D16_UNORM";
		case VK_FORMAT_X8_D24_UNORM_PACK32: return "VK_FORMAT_X8_D24_UNORM_PACK32";
		case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
		case VK_FORMAT_S8_UINT: return "VK_FORMAT_S8_UINT";
		case VK_FORMAT_D16_UNORM_S8_UINT: return "VK_FORMAT_D16_UNORM_S8_UINT";
		case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
		case VK_FORMAT_D32_SFLOAT_S8_UINT: return "VK_FORMAT_D32_SFLOAT_S8_UINT";
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return "VK_FORMAT_BC1_RGB_UNORM_BLOCK";
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return "VK_FORMAT_BC1_RGB_SRGB_BLOCK";
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return "VK_FORMAT_BC1_RGBA_UNORM_BLOCK";
		case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return "VK_FORMAT_BC1_RGBA_SRGB_BLOCK";
		case VK_FORMAT_BC2_UNORM_BLOCK: return "VK_FORMAT_BC2_UNORM_BLOCK";
		case VK_FORMAT_BC2_SRGB_BLOCK: return "VK_FORMAT_BC2_SRGB_BLOCK";
		case VK_FORMAT_BC3_UNORM_BLOCK: return "VK_FORMAT_BC3_UNORM_BLOCK";
		case VK_FORMAT_BC3_SRGB_BLOCK: return "VK_FORMAT_BC3_SRGB_BLOCK";
		case VK_FORMAT_BC4_UNORM_BLOCK: return "VK_FORMAT_BC4_UNORM_BLOCK";
		case VK_FORMAT_BC4_SNORM_BLOCK: return "VK_FORMAT_BC4_SNORM_BLOCK";
		case VK_FORMAT_BC5_UNORM_BLOCK: return "VK_FORMAT_BC5_UNORM_BLOCK";
		case VK_FORMAT_BC5_SNORM_BLOCK: return "VK_FORMAT_BC5_SNORM_BLOCK";
		case VK_FORMAT_BC6H_UFLOAT_BLOCK: return "VK_FORMAT_BC6H_UFLOAT_BLOCK";
		case VK_FORMAT_BC6H_SFLOAT_BLOCK: return "VK_FORMAT_BC6H_SFLOAT_BLOCK";
		case VK_FORMAT_BC7_UNORM_BLOCK: return "VK_FORMAT_BC7_UNORM_BLOCK";
		case VK_FORMAT_BC7_SRGB_BLOCK: return "VK_FORMAT_BC7_SRGB_BLOCK";
		case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK";
		case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK";
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK";
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK";
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK";
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK";
		case VK_FORMAT_EAC_R11_UNORM_BLOCK: return "VK_FORMAT_EAC_R11_UNORM_BLOCK";
		case VK_FORMAT_EAC_R11_SNORM_BLOCK: return "VK_FORMAT_EAC_R11_SNORM_BLOCK";
		case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return "VK_FORMAT_EAC_R11G11_UNORM_BLOCK";
		case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return "VK_FORMAT_EAC_R11G11_SNORM_BLOCK";
		case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return "VK_FORMAT_ASTC_4x4_UNORM_BLOCK";
		case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return "VK_FORMAT_ASTC_4x4_SRGB_BLOCK";
		case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return "VK_FORMAT_ASTC_5x4_UNORM_BLOCK";
		case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return "VK_FORMAT_ASTC_5x4_SRGB_BLOCK";
		case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_5x5_UNORM_BLOCK";
		case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_5x5_SRGB_BLOCK";
		case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_6x5_UNORM_BLOCK";
		case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_6x5_SRGB_BLOCK";
		case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_6x6_UNORM_BLOCK";
		case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_6x6_SRGB_BLOCK";
		case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x5_UNORM_BLOCK";
		case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x5_SRGB_BLOCK";
		case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x6_UNORM_BLOCK";
		case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x6_SRGB_BLOCK";
		case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x8_UNORM_BLOCK";
		case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x8_SRGB_BLOCK";
		case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x5_UNORM_BLOCK";
		case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x5_SRGB_BLOCK";
		case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x6_UNORM_BLOCK";
		case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x6_SRGB_BLOCK";
		case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x8_UNORM_BLOCK";
		case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x8_SRGB_BLOCK";
		case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x10_UNORM_BLOCK";
		case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x10_SRGB_BLOCK";
		case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return "VK_FORMAT_ASTC_12x10_UNORM_BLOCK";
		case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return "VK_FORMAT_ASTC_12x10_SRGB_BLOCK";
		case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return "VK_FORMAT_ASTC_12x12_UNORM_BLOCK";
		case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return "VK_FORMAT_ASTC_12x12_SRGB_BLOCK";
		case VK_FORMAT_G8B8G8R8_422_UNORM: return "VK_FORMAT_G8B8G8R8_422_UNORM";
		case VK_FORMAT_B8G8R8G8_422_UNORM: return "VK_FORMAT_B8G8R8G8_422_UNORM";
		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM";
		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM";
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: return "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM";
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM";
		case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM";
		case VK_FORMAT_R10X6_UNORM_PACK16: return "VK_FORMAT_R10X6_UNORM_PACK16";
		case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: return "VK_FORMAT_R10X6G10X6_UNORM_2PACK16";
		case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: return "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16";
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16";
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16";
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16";
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16";
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16";
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16";
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16";
		case VK_FORMAT_R12X4_UNORM_PACK16: return "VK_FORMAT_R12X4_UNORM_PACK16";
		case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: return "VK_FORMAT_R12X4G12X4_UNORM_2PACK16";
		case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: return "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16";
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16";
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16";
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16";
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16";
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16";
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16";
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16";
		case VK_FORMAT_G16B16G16R16_422_UNORM: return "VK_FORMAT_G16B16G16R16_422_UNORM";
		case VK_FORMAT_B16G16R16G16_422_UNORM: return "VK_FORMAT_B16G16R16G16_422_UNORM";
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: return "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM";
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM";
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: return "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM";
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: return "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM";
		case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM";
		case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM: return "VK_FORMAT_G8_B8R8_2PLANE_444_UNORM";
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16";
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16";
		case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM: return "VK_FORMAT_G16_B16R16_2PLANE_444_UNORM";
		case VK_FORMAT_A4R4G4B4_UNORM_PACK16: return "VK_FORMAT_A4R4G4B4_UNORM_PACK16";
		case VK_FORMAT_A4B4G4R4_UNORM_PACK16: return "VK_FORMAT_A4B4G4R4_UNORM_PACK16";
		case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK";
		case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK";
		case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG";
		case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG";
		case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG";
		case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG";
		case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG";
		case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG";
		case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG";
		case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG";
		case VK_FORMAT_R16G16_SFIXED5_NV: return "VK_FORMAT_R16G16_SFIXED5_NV";
		case VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR: return "VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR";
		case VK_FORMAT_A8_UNORM_KHR: return "VK_FORMAT_A8_UNORM_KHR";
		default: return "Unknown surface format";
	}
}

static const char* vulkan_get_color_space_name(VkColorSpaceKHR colorSpace)
{
	switch (colorSpace)
	{
		case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
		case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT";
		case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: return "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";
		case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT: return "VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT";
		case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT: return "VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT";
		case VK_COLOR_SPACE_BT709_LINEAR_EXT: return "VK_COLOR_SPACE_BT709_LINEAR_EXT";
		case VK_COLOR_SPACE_BT709_NONLINEAR_EXT: return "VK_COLOR_SPACE_BT709_NONLINEAR_EXT";
		case VK_COLOR_SPACE_BT2020_LINEAR_EXT: return "VK_COLOR_SPACE_BT2020_LINEAR_EXT";
		case VK_COLOR_SPACE_HDR10_ST2084_EXT: return "VK_COLOR_SPACE_HDR10_ST2084_EXT";
		case VK_COLOR_SPACE_DOLBYVISION_EXT: return "VK_COLOR_SPACE_DOLBYVISION_EXT";
		case VK_COLOR_SPACE_HDR10_HLG_EXT: return "VK_COLOR_SPACE_HDR10_HLG_EXT";
		case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT: return "VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT";
		case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT: return "VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT";
		case VK_COLOR_SPACE_PASS_THROUGH_EXT: return "VK_COLOR_SPACE_PASS_THROUGH_EXT";
		case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT: return "VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT";
		case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD: return "VK_COLOR_SPACE_DISPLAY_NATIVE_AMD";
		default: return "Unknown color space";
	}
}

static VkPhysicalDevice selectPhysicalDevice(
	uint32_t* mainQueueFamily, 
	VkSurfaceFormatKHR* bestSurfaceFormat,
	VkInstance instance, 
	VkSurfaceKHR surface,
	const VkSurfaceFormatKHR* surfaceFormatCandidates)
{
	VkResult r;

	VkPhysicalDevice devices[64];
	uint32_t deviceCount = countof(devices);
	r = vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
	if (r != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}
	assert(deviceCount <= countof(devices));

	VkPhysicalDevice selectedDevice;
	uint32_t selectedScore = 0u;

	for (uint32_t deviceIndex = 0u; deviceIndex < deviceCount; ++deviceIndex)
	{
		VkPhysicalDevice device = devices[deviceIndex];
		
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		printf("DEVICE: %s\n", properties.deviceName);

		if (properties.apiVersion < VK_API_VERSION_1_3) {
			printf("Device does not support Vulkan API version 1.3\n");
			continue;
		}

		VkPhysicalDeviceVulkan13Properties vulkan13Properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES };

		VkPhysicalDeviceProperties2 properties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		properties2.pNext = &vulkan13Properties;
		vkGetPhysicalDeviceProperties2(device, &properties2);
		
		VkPhysicalDeviceVulkan13Features vulkan13Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };

		VkPhysicalDeviceFeatures2 features2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		features2.pNext = &vulkan13Features;
		vkGetPhysicalDeviceFeatures2(device, &features2);

		if (vulkan13Features.dynamicRendering == VK_FALSE) {
			printf("Device does not support dynamic rendering\n");
			continue;
		}

		VkQueueFamilyProperties queueFamilies[64];
		uint32_t queueFamilyCount = countof(queueFamilies);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);
		assert(queueFamilyCount <= countof(queueFamilies));

		VkSurfaceFormatKHR surfaceFormats[64];
		uint32_t surfaceFormatCount = countof(surfaceFormats);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surfaceFormatCount, surfaceFormats);

		printf("Supported surface formats:\n");
		for (uint32_t i = 0; i < surfaceFormatCount; ++i)
		{
			const VkSurfaceFormatKHR* surfaceFormat = surfaceFormats + i;
			printf("\t%s (%s)\n", vulkan_get_format_name(surfaceFormat->format), vulkan_get_color_space_name(surfaceFormat->colorSpace));
		}

		uint32_t bestSurfaceFormatIndex = surfaceFormatCount;
		for (const VkSurfaceFormatKHR* haystack = surfaceFormatCandidates; haystack->format != VK_FORMAT_UNDEFINED; ++haystack)
		{
			for (uint32_t i = 0; i < bestSurfaceFormatIndex; ++i)
			{
				const VkSurfaceFormatKHR* needle = surfaceFormats + i;
				if (needle->format == haystack->format && 
					needle->colorSpace == haystack->colorSpace)
				{
					bestSurfaceFormatIndex = i;
					break;
				}
			}
		}
	
		if (bestSurfaceFormatIndex == surfaceFormatCount) {
			fprintf(stderr, "Device has no suitable surface format\n");
			continue;
		}
		
		char* nvidia = strstr(properties.deviceName, "NVIDIA");
		if (nvidia == NULL)
		{
			continue;
		}

		uint32_t selectedQueue = UINT32_MAX;
		for (uint32_t i = 0u; i < queueFamilyCount; ++i) {
			const uint32_t flags = queueFamilies[i].queueFlags;
			const bool has_graphics = flags & VK_QUEUE_GRAPHICS_BIT;
			const bool has_compute = flags & VK_QUEUE_COMPUTE_BIT;
			if (has_graphics && has_compute) {
				selectedQueue = i;
				break;
			}
		}

		if (selectedQueue == UINT32_MAX) {
			fprintf(stderr, "Device has no suitable graphics queue\n");
			continue;
		}

		uint32_t score = 0u;
		
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		if( score > selectedScore )
		{
			selectedDevice		= device;
			selectedScore		= score;
			*mainQueueFamily	= selectedQueue;
			*bestSurfaceFormat	= surfaceFormats[bestSurfaceFormatIndex];
		}
	}
	
	return selectedDevice;
}

int CreateVulkanContext(vulkan_t* vulkan, VkInstance instance, VkSurfaceKHR surface)
{
	VkResult r;

	const VkSurfaceFormatKHR supportedSurfaceFormats[] = {
		{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
		{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
		{ VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
		{ },
	};

	uint32_t mainQueueFamilyIndex;
	VkSurfaceFormatKHR surfaceFormat;

	VkPhysicalDevice physicalDevice = selectPhysicalDevice(
		&mainQueueFamilyIndex, 
		&surfaceFormat,
		instance, 
		surface,
		supportedSurfaceFormats);

	if (physicalDevice == NULL) {
		fprintf(stderr, "Failed to find a suitable vulkan device Sadeg\n");
		return 1;
	}

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	printf("SELECTED DEVICE: %s\n", deviceProperties.deviceName);

	const float queuePriorities[] = { 1.0f };
	const VkDeviceQueueCreateInfo queueInfos[] = {
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = mainQueueFamilyIndex,
			.queueCount = 1,
			.pQueuePriorities = queuePriorities,
		}
	};

	const char* deviceExtensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	VkPhysicalDeviceVulkan13Features vulkan13Features = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.dynamicRendering = VK_TRUE,
		.synchronization2 = VK_TRUE,
	};

	VkPhysicalDeviceVulkan12Features vulkan12Features = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &vulkan13Features,
	};

	VkPhysicalDeviceVulkan11Features vulkan11Features = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &vulkan12Features,
		.shaderDrawParameters = VK_TRUE,
	};

	VkPhysicalDeviceFeatures2 features2 = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &vulkan11Features,
		.features = {
			.multiDrawIndirect = VK_TRUE,
			.geometryShader = VK_TRUE,
			.shaderTessellationAndGeometryPointSize = VK_TRUE,
			.wideLines = VK_TRUE,
		},
	};

	VkDeviceCreateInfo deviceInfo = {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features2,
		.ppEnabledExtensionNames = deviceExtensions,
		.enabledExtensionCount = countof(deviceExtensions),
		.pQueueCreateInfos = queueInfos,
		.queueCreateInfoCount = countof(queueInfos),
	};

	VkDevice device;
	r = vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device);
	if (r != VK_SUCCESS)
	{
		fprintf(stderr, "vkCreateDevice failed\n",  r);
		return 1;
	}
	assert(device != VK_NULL_HANDLE);

	VkQueue mainQueue;
	vkGetDeviceQueue(device, mainQueueFamilyIndex, 0, &mainQueue);

	const VkCommandPoolCreateInfo commandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.queueFamilyIndex = mainQueueFamilyIndex,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
	};

	VkCommandPool commandPool;
	r = vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool);
	assert(r == VK_SUCCESS);
	
	*vulkan = (vulkan_t){
		.physicalDevice = physicalDevice,
		.physicalDeviceLimits = deviceProperties.limits,
		.device = device,
		.mainQueue = mainQueue,
		.mainQueueFamilyIndex = mainQueueFamilyIndex,
		.surfaceFormat = surfaceFormat,
		.commandPool = commandPool,
	};

	vulkan->vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");

	{
		const VkSamplerCreateInfo samplerCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.minFilter = VK_FILTER_NEAREST,
			.magFilter = VK_FILTER_NEAREST,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		};
		vkCreateSampler(device, &samplerCreateInfo, NULL, &vulkan->pointClampSampler);
	}
	{
		const VkSamplerCreateInfo samplerCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.minFilter = VK_FILTER_LINEAR,
			.magFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		};
		vkCreateSampler(device, &samplerCreateInfo, NULL, &vulkan->linearClampSampler);
	}

	return 0;
}

void DestroyVulkanContext(vulkan_t* vulkan)
{
	vkDestroySampler(vulkan->device, vulkan->pointClampSampler, NULL);
	vkDestroySampler(vulkan->device, vulkan->linearClampSampler, NULL);
	vkDestroyCommandPool(vulkan->device, vulkan->commandPool, NULL);
	vkDestroyDevice(vulkan->device, NULL);
}

uint32_t FindMemoryType(vulkan_t* vulkan, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vulkan->physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	
	assert(false && "Failed to find a suitable memory type");
	return UINT32_MAX;
}

VkBuffer CreateBuffer(VkDeviceMemory* memory, vulkan_t* vulkan, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
{
	VkResult r;

	const VkBufferCreateInfo bufferInfo = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
	};

	VkBuffer buffer;
	r = vkCreateBuffer(vulkan->device, &bufferInfo, NULL, &buffer);
	assert(r == VK_SUCCESS);

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(vulkan->device, buffer, &memoryRequirements);

	const VkMemoryAllocateInfo memoryAllocateInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = FindMemoryType(vulkan, memoryRequirements.memoryTypeBits, memoryProperties),
	};
	r = vkAllocateMemory(vulkan->device, &memoryAllocateInfo, NULL, memory);
	assert(r == VK_SUCCESS);
	
	r = vkBindBufferMemory(vulkan->device, buffer, *memory, 0);
	assert(r == VK_SUCCESS);

	return buffer;
}

VkImage CreateImage(VkDeviceMemory* memory, vulkan_t* vulkan, const VkImageCreateInfo* imageCreateInfo, VkMemoryPropertyFlags memoryProperties)
{
	VkResult r;

	VkImage image;
	r = vkCreateImage(vulkan->device, imageCreateInfo, NULL, &image);
	assert(r == VK_SUCCESS);

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(vulkan->device, image, &memoryRequirements);

	const VkMemoryAllocateInfo memoryAllocateInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = FindMemoryType(vulkan, memoryRequirements.memoryTypeBits, memoryProperties),
	};
	r = vkAllocateMemory(vulkan->device, &memoryAllocateInfo, NULL, memory);
	assert(r == VK_SUCCESS);
	
	r = vkBindImageMemory(vulkan->device, image, *memory, 0);
	assert(r == VK_SUCCESS);

	return image;
}

VkImage CreateImage2D(VkDeviceMemory* memory, vulkan_t* vulkan, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
{
	const VkImageCreateInfo imageInfo = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {
			.width = width,
			.height = height,
			.depth = 1,
		},
		.usage = usage,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.arrayLayers = 1,
		.mipLevels = 1,
	};
	return CreateImage(memory, vulkan, &imageInfo, memoryProperties);
}

void SetViewportAndScissor(VkCommandBuffer cb, VkOffset2D offset, VkExtent2D size)
{
	const VkViewport viewport = {
		.x = (float)offset.x,
		.y = (float)offset.y,
		.width = (float)size.width,
		.height = (float)size.height,
		.maxDepth = 1.0f,
	};
	const VkRect2D scissor = {
		.offset = offset,
		.extent = size,
	};
	vkCmdSetViewport(cb, 0, 1, &viewport);
	vkCmdSetScissor(cb, 0, 1, &scissor);
}

const char* VkResultString(VkResult result)
{
	switch (result)
	{
		case VK_SUCCESS: return "VK_SUCCESS";
		case VK_NOT_READY: return "VK_NOT_READY";
		case VK_TIMEOUT: return "VK_TIMEOUT";
		case VK_EVENT_SET: return "VK_EVENT_SET";
		case VK_EVENT_RESET: return "VK_EVENT_RESET";
		case VK_INCOMPLETE: return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
		case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
		case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
		case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
		case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
		case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
	#ifdef VK_ENABLE_BETA_EXTENSIONS
			case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
	#endif
	#ifdef VK_ENABLE_BETA_EXTENSIONS
			case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
	#endif
	#ifdef VK_ENABLE_BETA_EXTENSIONS
			case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
	#endif
	#ifdef VK_ENABLE_BETA_EXTENSIONS
			case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
	#endif
	#ifdef VK_ENABLE_BETA_EXTENSIONS
			case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
	#endif
	#ifdef VK_ENABLE_BETA_EXTENSIONS
			case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
	#endif
			case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
			case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
			case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
			case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
			case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
			case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
			case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
			case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
			case VK_RESULT_MAX_ENUM: return "VK_RESULT_MAX_ENUM";
			default: return "??????";
	}
}

static void SetObjectName(vulkan_t* vulkan, VkObjectType type, uint64_t handle, const char* name)
{
	if (vulkan->vkSetDebugUtilsObjectNameEXT == NULL) {
		return;
	}
	const VkDebugUtilsObjectNameInfoEXT objectNameInfo = {
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pObjectName = name,
		.objectType = type,
		.objectHandle = handle,
	};
	vulkan->vkSetDebugUtilsObjectNameEXT(vulkan->device, &objectNameInfo);
}

void SetShaderModuleName(vulkan_t* vulkan, VkShaderModule module, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)module, name);
}

void SetBufferName(vulkan_t* vulkan, VkBuffer buffer, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer, name);
}

void SetBufferViewName(vulkan_t* vulkan, VkBufferView bufferView, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_BUFFER_VIEW, (uint64_t)bufferView, name);
}

void SetImageName(vulkan_t* vulkan, VkImage image, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_IMAGE, (uint64_t)image, name);
}

void SetImageViewName(vulkan_t* vulkan, VkImageView imageView, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)imageView, name);
}

void SetDeviceMemoryName(vulkan_t* vulkan, VkDeviceMemory memory, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)memory, name);
}

void SetDescriptorSetName(vulkan_t* vulkan, VkDescriptorSet descriptorSet, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptorSet, name);
}

void SetDescriptorSetLayoutName(vulkan_t* vulkan, VkDescriptorSetLayout descriptorSetLayout, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)descriptorSetLayout, name);
}

void SetPipelineLayoutName(vulkan_t* vulkan, VkPipelineLayout pipelineLayout, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)pipelineLayout, name);
}

void SetPipelineName(vulkan_t* vulkan, VkPipeline pipeline, const char* name)
{
	SetObjectName(vulkan, VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, name);
}