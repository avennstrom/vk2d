#pragma once

#include "vulkan.h"

typedef struct descriptor_allocator descriptor_allocator_t;
typedef struct render_targets render_targets_t;
typedef struct composite composite_t;

composite_t* composite_create(vulkan_t* vulkan);
void composite_destroy(composite_t* composite, vulkan_t* vulkan);

void draw_composite(
	VkCommandBuffer cb,
	vulkan_t* vulkan,
	composite_t* composite,
	descriptor_allocator_t* dsalloc,
	render_targets_t* rt,
	VkImageView backbuffer);