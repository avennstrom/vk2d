#include "wind.h"
#include "types.h"
#include "vec.h"
#include "common.h"
#include "debug_renderer.h"
#include "../shaders/gpu_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WIND_GRID_CELL_COUNT	(WIND_GRID_RESOLUTION*WIND_GRID_RESOLUTION)
#define WIND_GRID_BUFFER_SIZE	(WIND_GRID_CELL_COUNT*sizeof(vec2))

typedef struct wind_frame
{
	VkBuffer		stagingBuffer;
	void*			stagingMemory;
} wind_frame_t;

typedef struct wind
{
	vulkan_t*		vk;
	vec2			gridVel[WIND_GRID_CELL_COUNT];
	//vec2			gridOrigin;
	int2			gridOrigin;
	VkBuffer		gridBuffer;
	VkDeviceMemory	gridBufferMemory;
	wind_frame_t	frames[FRAME_COUNT];
} wind_t;

wind_t* wind_create(vulkan_t* vulkan)
{
	wind_t* wind = calloc(1, sizeof(wind_t));
	if (wind == NULL)
	{
		return NULL;
	}

	wind->vk = vulkan;

	//wind->gridOrigin = (vec2){-5.0f, -5.0f};
	
	wind->gridBuffer = CreateBuffer(
		&wind->gridBufferMemory, 
		vulkan,
		WIND_GRID_BUFFER_SIZE,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	return wind;
}

void wind_destroy(wind_t* wind)
{
	vulkan_t* vk = wind->vk;

	vkDestroyBuffer(vk->device, wind->gridBuffer, NULL);
	vkFreeMemory(vk->device, wind->gridBufferMemory, NULL);

	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		vkDestroyBuffer(vk->device, wind->frames[i].stagingBuffer, NULL);
	}

	free(wind);
}

int wind_alloc_staging_mem(staging_memory_allocator_t* allocator, wind_t* wind)
{
	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		wind_frame_t* frame = &wind->frames[i];

		PushStagingBufferAllocation(
			allocator, 
			&frame->stagingBuffer,
			(void**)&frame->stagingMemory,
			WIND_GRID_BUFFER_SIZE,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			"Wind Staging");
	}

	return 0;
}

void wind_tick(wind_t* wind)
{
	for (int i = 0; i < WIND_GRID_CELL_COUNT; ++i)
	{
		vec2 vel = wind->gridVel[i];
		vel = vec2_scale(vel, 0.85f);
		wind->gridVel[i] = vel;
	}
}

void wind_update(VkCommandBuffer cb, wind_t* wind, const render_context_t* rc)
{
#if 0
	for (int x = 0; x < WIND_GRID_RESOLUTION + 1; ++x)
	{
		const int ox = x + wind->gridOrigin.x - (WIND_GRID_RESOLUTION/2);
		DrawDebugLine(
			(debug_vertex_t){.x = ox*WIND_GRID_CELL_SIZE, .y = (wind->gridOrigin.y - (WIND_GRID_RESOLUTION/2)) * WIND_GRID_CELL_SIZE, .color = 0xff808080},
			(debug_vertex_t){.x = ox*WIND_GRID_CELL_SIZE, .y = (wind->gridOrigin.y + (WIND_GRID_RESOLUTION/2)) * WIND_GRID_CELL_SIZE, .color = 0xff808080}
		);
	}
	for (int y = 0; y < WIND_GRID_RESOLUTION + 1; ++y)
	{
		const int oy = y + wind->gridOrigin.y - (WIND_GRID_RESOLUTION/2);
		DrawDebugLine(
			(debug_vertex_t){.y = oy*WIND_GRID_CELL_SIZE, .x = (wind->gridOrigin.x - (WIND_GRID_RESOLUTION/2)) * WIND_GRID_CELL_SIZE, .color = 0xff808080},
			(debug_vertex_t){.y = oy*WIND_GRID_CELL_SIZE, .x = (wind->gridOrigin.x + (WIND_GRID_RESOLUTION/2)) * WIND_GRID_CELL_SIZE, .color = 0xff808080}
		);
	}
#endif
#if 1
	for (int y = 0; y < WIND_GRID_RESOLUTION; ++y)
	{
		for (int x = 0; x < WIND_GRID_RESOLUTION; ++x)
		{
			const int dx = (x - WIND_GRID_RESOLUTION/2) + wind->gridOrigin.x;
			const int dy = (y - WIND_GRID_RESOLUTION/2) + wind->gridOrigin.y;
			
			const int i = (((uint)dx) % WIND_GRID_RESOLUTION) + (((uint)dy) % WIND_GRID_RESOLUTION) * WIND_GRID_RESOLUTION;
			const vec2 vel = vec2_scale(wind->gridVel[i], 2.0f);
			
			const vec2 center = {
				dx*WIND_GRID_CELL_SIZE + WIND_GRID_CELL_SIZE*0.5f,
				dy*WIND_GRID_CELL_SIZE + WIND_GRID_CELL_SIZE*0.5f
			};

			DrawDebugLine(
				(debug_vertex_t){.x = center.x, .y = center.y, .color = 0xff0000ff},
				(debug_vertex_t){.x = center.x + vel.x, .y = center.y + vel.y, .color = 0xff0000ff}
			);
		}
	}
	// for (int i = 0; i < WIND_GRID_CELL_COUNT; ++i)
	// {
	// 	const int x = i % WIND_GRID_RESOLUTION;
	// 	const int y = i / WIND_GRID_RESOLUTION;
	// 	const vec2 vel = wind->gridVel[i];
		
	// 	const vec2 center = {
	// 		x*WIND_GRID_CELL_SIZE + WIND_GRID_CELL_SIZE*0.5f,
	// 		y*WIND_GRID_CELL_SIZE + WIND_GRID_CELL_SIZE*0.5f
	// 	};
		
	// 	DrawDebugLine(
	// 		(debug_vertex_t){.x = center.x, .y = center.y, .color = 0xff0000ff},
	// 		(debug_vertex_t){.x = center.x + vel.x, .y = center.y + vel.y, .color = 0xff0000ff}
	// 	);
	// }
#endif

	wind_frame_t* frame = &wind->frames[rc->frameIndex];
	memcpy(frame->stagingMemory, wind->gridVel, WIND_GRID_BUFFER_SIZE);
	PushStagingMemoryFlush(rc->stagingMemory, frame->stagingMemory, WIND_GRID_BUFFER_SIZE);
	
	const VkBufferCopy copyRegion = {
		.size = WIND_GRID_BUFFER_SIZE,
	};
	vkCmdCopyBuffer(cb, frame->stagingBuffer, wind->gridBuffer, 1, &copyRegion);
}

void wind_inject(wind_t* wind, wind_injection_t injection)
{
	if (injection.vel.x == 0.0f && 
		injection.vel.y == 0.0f)
	{
		return;
	}

	int x_min = (int)(floorf((injection.aabbMin.x) / WIND_GRID_CELL_SIZE));
	int y_min = (int)(floorf((injection.aabbMin.y) / WIND_GRID_CELL_SIZE));
	int x_max = (int)(ceilf((injection.aabbMax.x) / WIND_GRID_CELL_SIZE));
	int y_max = (int)(ceilf((injection.aabbMax.y) / WIND_GRID_CELL_SIZE));
	
	for (int y = y_min; y < y_max; ++y)
	{
		for (int x = x_min; x < x_max; ++x)
		{
			const uint wrapX = ((uint)x) % WIND_GRID_RESOLUTION;
			const uint wrapY = ((uint)y) % WIND_GRID_RESOLUTION;

			const uint i = wrapX + wrapY * WIND_GRID_RESOLUTION;

			vec2 vel = wind->gridVel[i];
			vel = vec2_add(vel, vec2_scale(injection.vel, 1.5f));
			if (vec2_length(vel) > 1.0f)
			{
				vel = vec2_normalize(vel);
			}
			wind->gridVel[i] = vel;
		}
	}
}

void wind_set_focus(wind_t* wind, vec2 pos)
{
	wind->gridOrigin.x = (pos.x / WIND_GRID_CELL_SIZE);
	wind->gridOrigin.y = (pos.y / WIND_GRID_CELL_SIZE);
}

void wind_get_render_info(wind_render_info_t* info, wind_t* wind)
{
	info->gridBuffer	= wind->gridBuffer;
}