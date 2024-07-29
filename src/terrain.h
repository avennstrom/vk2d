#pragma once

#include "types.h"
#include "vulkan.h"
#include "staging_memory.h"
#include "render_context.h"

#include <stdbool.h>

typedef struct terrain terrain_t;

terrain_t* terrain_create(vulkan_t* vulkan);
void terrain_destroy(terrain_t* terrain);
void terrain_alloc_staging_mem(staging_memory_allocator_t* allocator, terrain_t* terrain);
void terrain_update(VkCommandBuffer cb, terrain_t* terrain, const render_context_t* rc);

typedef struct terrain_info {
	VkBuffer		indexBuffer;
	uint			indexCount;
	VkBuffer		heightBuffer;
	VkBuffer		normalBuffer;
} terrain_info_t;

bool terrain_get_info(terrain_info_t* info, const terrain_t* terrain);