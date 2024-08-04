#include "world.h"
#include "types.h"
#include "debug_renderer.h"

#include <stdlib.h>

#define WORLD_MAX_INDEX_COUNT (16 * 1024)
#define WORLD_MAX_VERTEX_COUNT (16 * 1024)
#define WORLD_INDEX_BUFFER_SIZE (WORLD_MAX_INDEX_COUNT * sizeof(uint16_t))
#define WORLD_POSITION_BUFFER_SIZE (WORLD_MAX_VERTEX_COUNT * sizeof(vec3))
#define WORLD_COLOR_BUFFER_SIZE (WORLD_MAX_VERTEX_COUNT * sizeof(uint32_t))
#define WORLD_STAGING_BUFFER_SIZE (WORLD_INDEX_BUFFER_SIZE + WORLD_POSITION_BUFFER_SIZE + WORLD_COLOR_BUFFER_SIZE)

#define WORLD_MAX_TRIANGLE_COLLIDERS 1024

typedef enum world_state
{
	WORLD_STATE_UPLOAD_TRIANGLES,
	WORLD_STATE_DONE,
} world_state_t;

static void triangle_collider_debug_draw(triangle_collider_t* t);

typedef struct world_colliders
{
	uint32_t			triangleCount;
	triangle_collider_t	triangles[WORLD_MAX_TRIANGLE_COLLIDERS];
} world_colliders_t;

typedef struct world
{
	vulkan_t*			vulkan;
	world_state_t		state;

	VkBuffer			indexBuffer;
	VkDeviceMemory		indexBufferMemory;
	VkBuffer			vertexPositionBuffer;
	VkDeviceMemory		vertexPositionBufferMemory;
	VkBuffer			vertexColorBuffer;
	VkDeviceMemory		vertexColorBufferMemory;
	uint32_t			indexCount;

	VkBuffer			stagingBuffer;
	void*				stagingBufferMemory;

	world_colliders_t	colliders;
} world_t;

world_t* world_create(vulkan_t* vulkan)
{
	world_t* world = calloc(1, sizeof(world_t));
	if (world == NULL)
	{
		return NULL;
	}

	world->vulkan = vulkan;
	
	world->indexBuffer = CreateBuffer(
		&world->indexBufferMemory,
		vulkan,
		WORLD_INDEX_BUFFER_SIZE,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	world->vertexPositionBuffer = CreateBuffer(
		&world->vertexPositionBufferMemory,
		vulkan,
		WORLD_POSITION_BUFFER_SIZE,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	world->vertexColorBuffer = CreateBuffer(
		&world->vertexColorBufferMemory,
		vulkan,
		WORLD_COLOR_BUFFER_SIZE,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	{
		world_colliders_t* colliders = &world->colliders;

		colliders->triangles[0] = (triangle_collider_t){
			{0.0f, 0.1f},
			{5.0f, 0.1f},
			{0.0f, -1.0f},
		};
		colliders->triangles[1] = (triangle_collider_t){
			{0.0f, -1.0f},
			{5.0f, 0.1f},
			{5.0f, -1.0f},
		};
		colliders->triangles[2] = (triangle_collider_t){
			{-5.0f, 1.0f},
			{0.0f, 0.1f},
			{0.0f, -1.0f},
		};
		colliders->triangles[3] = (triangle_collider_t){
			{0.0f, -1.0f},
			{-5.0f, -1.0f},
			{-5.0f, 1.0f},
		};
		colliders->triangleCount = 4;
	}

	return world;
}

void world_destroy(world_t* world)
{
	vulkan_t* vulkan = world->vulkan;

	vkDestroyBuffer(vulkan->device, world->stagingBuffer, NULL);

	vkDestroyBuffer(vulkan->device, world->indexBuffer, NULL);
	vkDestroyBuffer(vulkan->device, world->vertexPositionBuffer, NULL);
	vkDestroyBuffer(vulkan->device, world->vertexColorBuffer, NULL);

	vkFreeMemory(vulkan->device, world->indexBufferMemory, NULL);
	vkFreeMemory(vulkan->device, world->vertexPositionBufferMemory, NULL);
	vkFreeMemory(vulkan->device, world->vertexColorBufferMemory, NULL);

	free(world);
}

void world_alloc_staging_mem(staging_memory_allocator_t* allocator, world_t* world)
{
	PushStagingBufferAllocation(
		allocator,
		&world->stagingBuffer,
		&world->stagingBufferMemory,
		WORLD_STAGING_BUFFER_SIZE,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		"World");
}

typedef struct primitive_context
{
	uint16_t*	indices;
	vec3*		positions;
	uint32_t*	colors;
	
	uint32_t	indexCount;
	uint32_t	vertexCount;
} primitive_context_t;

static void grow_grass(primitive_context_t* ctx, vec2 root)
{
	ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 0;
	ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 1;
	ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 2;
	ctx->indexCount += 3;

	float width = 0.05f;
	float height = 0.1f + (rand() / (float)RAND_MAX) * 0.5f;

	ctx->positions[ctx->vertexCount + 0] = (vec3){root.x, root.y + height};
	ctx->positions[ctx->vertexCount + 1] = (vec3){root.x - width * 0.5f, root.y};
	ctx->positions[ctx->vertexCount + 2] = (vec3){root.x + width * 0.5f, root.y};

	uint8_t red = rand() & 0b1111111;

	ctx->colors[ctx->vertexCount + 0] = 0xff00a000 | red;
	ctx->colors[ctx->vertexCount + 1] = 0x00002000 | (red/2);
	ctx->colors[ctx->vertexCount + 2] = 0x00002000 | (red/2);

	ctx->vertexCount += 3;
}

static void grow_flower(primitive_context_t* ctx, vec2 root)
{
	float width = 0.02f;
	float height = 0.3f + (rand() / (float)RAND_MAX) * 0.5f;
	float headSize = 0.1f;

	// stem
	{
		ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 0;
		ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 3] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 4] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 5] = ctx->vertexCount + 3;
		ctx->indexCount += 6;

		ctx->positions[ctx->vertexCount + 0] = (vec3){root.x - width * 0.5f, root.y + height};
		ctx->positions[ctx->vertexCount + 1] = (vec3){root.x + width * 0.5f, root.y + height};
		ctx->positions[ctx->vertexCount + 2] = (vec3){root.x - width * 0.5f, root.y};
		ctx->positions[ctx->vertexCount + 3] = (vec3){root.x + width * 0.5f, root.y};

		uint8_t red = rand() & 0b11111;

		ctx->colors[ctx->vertexCount + 0] = 0xff007000 | red;
		ctx->colors[ctx->vertexCount + 1] = 0xff007000 | red;
		ctx->colors[ctx->vertexCount + 2] = 0x00001000 | (red/2);
		ctx->colors[ctx->vertexCount + 3] = 0x00001000 | (red/2);

		ctx->vertexCount += 4;
	}

	// head
	{

		ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 0;
		ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 3] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 4] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 5] = ctx->vertexCount + 3;
		ctx->indexCount += 6;

		ctx->positions[ctx->vertexCount + 0] = (vec3){root.x - headSize * 0.5f, root.y + height + headSize * 0.5f};
		ctx->positions[ctx->vertexCount + 1] = (vec3){root.x + headSize * 0.5f, root.y + height + headSize * 0.5f};
		ctx->positions[ctx->vertexCount + 2] = (vec3){root.x - headSize * 0.2f, root.y + height - headSize * 0.5f};
		ctx->positions[ctx->vertexCount + 3] = (vec3){root.x + headSize * 0.2f, root.y + height - headSize * 0.5f};

		uint8_t r = rand() & 0xffu;
		uint8_t g = rand() & 0xffu;
		uint8_t b = rand() & 0xffu;

		uint32_t color = r | (g << 8) | (b << 16);
		uint32_t darkColor = (r/2) | ((g/2) << 8) | ((b/2) << 16);

		ctx->colors[ctx->vertexCount + 0] = 0xff000000 | color;
		ctx->colors[ctx->vertexCount + 1] = 0xff000000 | color;
		ctx->colors[ctx->vertexCount + 2] = 0xff000000 | darkColor;
		ctx->colors[ctx->vertexCount + 3] = 0xff000000 | darkColor;
		ctx->vertexCount += 4;
	}
}

static void grow_plant(primitive_context_t* ctx, vec2 root)
{
	const int type = rand() % 2;
	if (type == 0)
	{
		grow_grass(ctx, root);
	}
	else if (type == 1)
	{
		grow_flower(ctx, root);
	}
}

static void fill_primitive_data(primitive_context_t* ctx)
{
	{
		ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 0;
		ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 3] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 4] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 5] = ctx->vertexCount + 3;
		ctx->indexCount += 6;

		ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 3;
		ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 4;
		ctx->indices[ctx->indexCount + 3] = ctx->vertexCount + 4;
		ctx->indices[ctx->indexCount + 4] = ctx->vertexCount + 3;
		ctx->indices[ctx->indexCount + 5] = ctx->vertexCount + 5;
		ctx->indexCount += 6;
		
		ctx->positions[ctx->vertexCount + 0] = (vec3){0.0f, 0.1f};
		ctx->positions[ctx->vertexCount + 1] = (vec3){5.0f, 0.1f};
		ctx->positions[ctx->vertexCount + 2] = (vec3){0.0f, 0.0f};
		ctx->positions[ctx->vertexCount + 3] = (vec3){5.0f, 0.0f};
		ctx->positions[ctx->vertexCount + 4] = (vec3){0.0f, -1.0f};
		ctx->positions[ctx->vertexCount + 5] = (vec3){5.0f, -1.0f};

		ctx->colors[ctx->vertexCount + 0] = 0x00a000;
		ctx->colors[ctx->vertexCount + 1] = 0x00a000;
		ctx->colors[ctx->vertexCount + 2] = 0x001020;
		ctx->colors[ctx->vertexCount + 3] = 0x001020;
		ctx->colors[ctx->vertexCount + 4] = 0x000a10;
		ctx->colors[ctx->vertexCount + 5] = 0x000a10;
		ctx->vertexCount += 6;
	}
	{
		ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 0;
		ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 3] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 4] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 5] = ctx->vertexCount + 3;
		ctx->indexCount += 6;

		ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 3;
		ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 4;
		ctx->indices[ctx->indexCount + 3] = ctx->vertexCount + 4;
		ctx->indices[ctx->indexCount + 4] = ctx->vertexCount + 3;
		ctx->indices[ctx->indexCount + 5] = ctx->vertexCount + 5;
		ctx->indexCount += 6;
		
		ctx->positions[ctx->vertexCount + 0] = (vec3){-5.0f, 1.0f};
		ctx->positions[ctx->vertexCount + 1] = (vec3){ 0.0f, 0.1f};
		ctx->positions[ctx->vertexCount + 2] = (vec3){-5.0f, 0.9f};
		ctx->positions[ctx->vertexCount + 3] = (vec3){ 0.0f, 0.0f};
		ctx->positions[ctx->vertexCount + 4] = (vec3){-5.0f, -1.0f};
		ctx->positions[ctx->vertexCount + 5] = (vec3){ 0.0f, -1.0f};

		ctx->colors[ctx->vertexCount + 0] = 0x00a000;
		ctx->colors[ctx->vertexCount + 1] = 0x00a000;
		ctx->colors[ctx->vertexCount + 2] = 0x001020;
		ctx->colors[ctx->vertexCount + 3] = 0x001020;
		ctx->colors[ctx->vertexCount + 4] = 0x000a10;
		ctx->colors[ctx->vertexCount + 5] = 0x000a10;
		ctx->vertexCount += 6;
	}

	for (int i = 0; i < 512; ++i)
	{
		float x = (rand() / (float)RAND_MAX) * 5.0f;
		grow_plant(ctx, (vec2){x, 0.1f});
	}
}

void world_update(world_t* world, VkCommandBuffer cb, const render_context_t* rc)
{
	switch (world->state)
	{
		case WORLD_STATE_UPLOAD_TRIANGLES:
		{
			uint16_t* stagingIndices = (uint16_t*)world->stagingBufferMemory;
			vec3* stagingPositions = (vec3*)((uint8_t*)world->stagingBufferMemory + WORLD_INDEX_BUFFER_SIZE);
			uint32_t* stagingColors = (uint32_t*)((uint8_t*)world->stagingBufferMemory + WORLD_INDEX_BUFFER_SIZE + WORLD_POSITION_BUFFER_SIZE);

			primitive_context_t ctx = {
				.indices = stagingIndices,
				.positions = stagingPositions,
				.colors = stagingColors,
			};

			fill_primitive_data(&ctx);

			world->indexCount = ctx.indexCount;
			
			PushStagingMemoryFlush(rc->stagingMemory, world->stagingBufferMemory, WORLD_STAGING_BUFFER_SIZE);
			
			{
				const VkBufferCopy copyRegion = {
					.size = WORLD_INDEX_BUFFER_SIZE,
				};
				vkCmdCopyBuffer(cb, world->stagingBuffer, world->indexBuffer, 1, &copyRegion);
			}
			{
				const VkBufferCopy copyRegion = {
					.size = WORLD_POSITION_BUFFER_SIZE,
					.srcOffset = WORLD_INDEX_BUFFER_SIZE,
				};
				vkCmdCopyBuffer(cb, world->stagingBuffer, world->vertexPositionBuffer, 1, &copyRegion);
			}
			{
				const VkBufferCopy copyRegion = {
					.size = WORLD_COLOR_BUFFER_SIZE,
					.srcOffset = WORLD_INDEX_BUFFER_SIZE + WORLD_POSITION_BUFFER_SIZE,
				};
				vkCmdCopyBuffer(cb, world->stagingBuffer, world->vertexColorBuffer, 1, &copyRegion);
			}

			world->state = WORLD_STATE_DONE;
			break;
		}
		
		case WORLD_STATE_DONE:
		{
			break;
		}
	}
	
#if 0
	for (size_t i = 0; i < world->colliders.triangleCount; ++i)
	{
		triangle_collider_debug_draw(&world->colliders.triangles[i]);
	}
#endif
}

bool world_get_render_info(world_render_info_t* info, world_t* world)
{
	if (world->state != WORLD_STATE_DONE)
	{
		return false;
	}

	info->indexBuffer			= world->indexBuffer;
	info->vertexPositionBuffer	= world->vertexPositionBuffer;
	info->vertexColorBuffer		= world->vertexColorBuffer;
	info->indexCount			= world->indexCount;

	return true;
}

void world_get_collision_info(world_collision_info_t* info, world_t* world)
{
	info->triangleCount	= world->colliders.triangleCount;
	info->triangles		= world->colliders.triangles;
}

static void triangle_collider_debug_draw(triangle_collider_t* t)
{
	const debug_vertex_t v0 = { .x = t->a.x, .y = t->a.y, .color = 0xffffffff };
	const debug_vertex_t v1 = { .x = t->b.x, .y = t->b.y, .color = 0xffffffff };
	const debug_vertex_t v2 = { .x = t->c.x, .y = t->c.y, .color = 0xffffffff };
	
	DrawDebugLine(v0, v1);
	DrawDebugLine(v1, v2);
	DrawDebugLine(v2, v0);
}