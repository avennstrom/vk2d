#include "render_targets.h"
#include "common.h"

#include <assert.h>
#include <stdio.h>

int AllocateDedicatedRenderTarget2D(
	dedicated_render_target_t *rt,
	vulkan_t *vulkan,
	VkExtent2D resolution,
	VkFormat format,
	VkImageUsageFlags usage,
	const char* debugName)
{
	VkResult vkr;

	VkImageAspectFlags imageAspect = 0;
	switch (format) {
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D32_SFLOAT:
			imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
			imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		default:
			imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	const VkImageCreateInfo imageInfo = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {
			.width = resolution.width,
			.height = resolution.height,
			.depth = 1,
		},
		.usage = usage,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.arrayLayers = 1,
		.mipLevels = 1,
	};
	rt->image = CreateImage(&rt->memory, vulkan, &imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	const VkImageViewCreateInfo imageViewInfo = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = rt->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = {
			.aspectMask = imageAspect,
			.levelCount = 1,
			.layerCount = 1,
		},
	};
	vkr = vkCreateImageView(vulkan->device, &imageViewInfo, NULL, &rt->view);
	assert(vkr == VK_SUCCESS);

	SetImageName(vulkan, rt->image, debugName);
	SetImageViewName(vulkan, rt->view, debugName);
	SetDeviceMemoryName(vulkan, rt->memory, debugName);

	return 0;
}

void destroy_dedicated_render_target(
	dedicated_render_target_t* rt,
	vulkan_t *vulkan)
{
	vkDestroyImageView(vulkan->device, rt->view, NULL);
	vkDestroyImage(vulkan->device, rt->image, NULL);
	vkFreeMemory(vulkan->device, rt->memory, NULL);

	rt->view = NULL;
	rt->image = NULL;
	rt->memory = NULL;
}

int CreateRenderTargets(
	render_targets_t *rt,
	vulkan_t *vulkan,
	VkExtent2D resolution)
{
	AllocateDedicatedRenderTarget2D(
		&rt->sceneColor, 
		vulkan, 
		resolution, 
		SCENE_COLOR_FORMAT, 
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
		"SceneColor");

	AllocateDedicatedRenderTarget2D(
		&rt->sceneDepth, 
		vulkan, 
		resolution, 
		SCENE_DEPTH_FORMAT, 
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
		"SceneDepth");
		
	{
		const VkImageCreateInfo imageInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_D16_UNORM,
			.extent = {
				.width = SPOT_LIGHT_RESOLUTION,
				.height = SPOT_LIGHT_RESOLUTION,
				.depth = 1,
			},
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.arrayLayers = MAX_SPOT_LIGHTS,
			.mipLevels = 1,
		};
		rt->spotLightAtlas.image = CreateImage(&rt->spotLightAtlas.memory, vulkan, &imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		const VkImageViewCreateInfo imageViewInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = rt->spotLightAtlas.image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			.format = VK_FORMAT_D16_UNORM,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.levelCount = 1,
				.layerCount = MAX_SPOT_LIGHTS,
			},
		};
		VkResult vkr = vkCreateImageView(vulkan->device, &imageViewInfo, NULL, &rt->spotLightAtlas.view);
		assert(vkr == VK_SUCCESS);

		SetImageName(vulkan, rt->spotLightAtlas.image, "SpotLightAtlas");
		SetImageViewName(vulkan, rt->spotLightAtlas.view, "SpotLightAtlas");
		SetDeviceMemoryName(vulkan, rt->spotLightAtlas.memory, "SpotLightAtlas");
	}

	rt->resolution = resolution;
	return 0;
}

void DestroyRenderTargets(
	render_targets_t *rt, 
	vulkan_t *vulkan)
{
	destroy_dedicated_render_target(&rt->sceneColor, vulkan);
	destroy_dedicated_render_target(&rt->sceneDepth, vulkan);

	rt->resolution = (VkExtent2D){0, 0};
}
