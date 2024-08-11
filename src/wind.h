#pragma once

#include "types.h"
#include "vulkan.h"
#include "staging_memory.h"
#include "render_context.h"

typedef struct wind wind_t;

wind_t* wind_create(vulkan_t* vulkan);
void wind_destroy(wind_t* wind);

int wind_alloc_staging_mem(staging_memory_allocator_t* allocator, wind_t* wind);

void wind_update(VkCommandBuffer cb, wind_t* wind, const render_context_t* rc, float deltaTime);

typedef struct wind_injection
{
	vec2	aabbMin;
	vec2	aabbMax;
	vec2	vel;
} wind_injection_t;

void wind_inject(wind_t* wind, wind_injection_t injection);

typedef struct wind_render_info
{
	VkBuffer	gridBuffer;
	vec2		gridOrigin;
} wind_render_info_t;

void wind_get_render_info(wind_render_info_t* info, wind_t* wind);
