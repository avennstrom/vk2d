#pragma once

#include "vulkan.h"
#include "render_context.h"
#include "staging_memory.h"

#include <stdbool.h>

typedef struct world world_t;

world_t* world_create(vulkan_t* vulkan);
void world_destroy(world_t* world);

void world_alloc_staging_mem(staging_memory_allocator_t* allocator, world_t* world);

void world_update(world_t* world, VkCommandBuffer cb, const render_context_t* rc);

typedef struct world_render_info
{
	uint32_t	indexCount;
	VkBuffer	indexBuffer;
	VkBuffer	vertexPositionBuffer;
	VkBuffer	vertexColorBuffer;
} world_render_info_t;

bool world_get_render_info(world_render_info_t* info, world_t* world);