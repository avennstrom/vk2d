#pragma once

#include "types.h"
#include "vulkan.h"

#define SCENE_COLOR_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
#define SCENE_DEPTH_FORMAT VK_FORMAT_D16_UNORM
#define SPOT_LIGHT_RESOLUTION 256

typedef struct dedicated_render_target {
	VkDeviceMemory memory;
	VkImage image;
	VkImageView view;
	image_state_t state;
} dedicated_render_target_t;

typedef struct render_targets {
	uint2 resolution;
	dedicated_render_target_t sceneDepth;
	dedicated_render_target_t sceneColor;
} render_targets_t;

int AllocateDedicatedRenderTarget2D(
	dedicated_render_target_t *rt,
	vulkan_t *vulkan,
	uint2 resolution,
	VkFormat format,
	VkImageUsageFlags usage,
	const char* debugName);

void destroy_dedicated_render_target(
	dedicated_render_target_t* rt,
	vulkan_t *vulkan);

int render_targets_create(
	render_targets_t *rt,
	vulkan_t *vulkan,
	uint2 resolution);

void render_targets_destroy(
	render_targets_t *rt,
	vulkan_t *vulkan);
