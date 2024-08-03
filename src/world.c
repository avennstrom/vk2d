#include "world.h"
#include "types.h"

#include <stdlib.h>

#define WORLD_MAX_INDEX_COUNT (16 * 1024)
#define WORLD_MAX_VERTEX_COUNT (16 * 1024)
#define WORLD_INDEX_BUFFER_SIZE (WORLD_MAX_INDEX_COUNT * sizeof(uint16_t))
#define WORLD_POSITION_BUFFER_SIZE (WORLD_MAX_VERTEX_COUNT * sizeof(vec3))
#define WORLD_COLOR_BUFFER_SIZE (WORLD_MAX_VERTEX_COUNT * sizeof(uint32_t))
#define WORLD_STAGING_BUFFER_SIZE (WORLD_INDEX_BUFFER_SIZE + WORLD_POSITION_BUFFER_SIZE + WORLD_COLOR_BUFFER_SIZE)

typedef enum world_state
{
	WORLD_STATE_UPLOAD_TRIANGLES,
	WORLD_STATE_DONE,
} world_state_t;

typedef struct world
{
	vulkan_t*		vulkan;
	world_state_t	state;

	VkBuffer		indexBuffer;
	VkDeviceMemory	indexBufferMemory;
	VkBuffer		vertexPositionBuffer;
	VkDeviceMemory	vertexPositionBufferMemory;
	VkBuffer		vertexColorBuffer;
	VkDeviceMemory	vertexColorBufferMemory;
	uint32_t		indexCount;

	VkBuffer		stagingBuffer;
	void*			stagingBufferMemory;
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

static void grow_plant(primitive_context_t* ctx, vec2 root)
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
	ctx->colors[ctx->vertexCount + 1] = 0xff002000 | (red/2);
	ctx->colors[ctx->vertexCount + 2] = 0xff002000 | (red/2);

	ctx->vertexCount += 3;
}

static void fill_primitive_data(primitive_context_t* ctx)
{
	ctx->indices[ctx->indexCount + 0] = 0;
	ctx->indices[ctx->indexCount + 1] = 1;
	ctx->indices[ctx->indexCount + 2] = 2;
	ctx->indices[ctx->indexCount + 3] = 2;
	ctx->indices[ctx->indexCount + 4] = 1;
	ctx->indices[ctx->indexCount + 5] = 3;
	ctx->indexCount += 6;

	ctx->indices[ctx->indexCount + 0] = 2;
	ctx->indices[ctx->indexCount + 1] = 3;
	ctx->indices[ctx->indexCount + 2] = 4;
	ctx->indices[ctx->indexCount + 3] = 4;
	ctx->indices[ctx->indexCount + 4] = 3;
	ctx->indices[ctx->indexCount + 5] = 5;
	ctx->indexCount += 6;
	
	ctx->positions[ctx->vertexCount + 0] = (vec3){0.0f, 0.1f};
	ctx->positions[ctx->vertexCount + 1] = (vec3){5.0f, 0.1f};
	ctx->positions[ctx->vertexCount + 2] = (vec3){0.0f, 0.0f};
	ctx->positions[ctx->vertexCount + 3] = (vec3){5.0f, 0.0f};
	ctx->positions[ctx->vertexCount + 4] = (vec3){0.0f, -1.0f};
	ctx->positions[ctx->vertexCount + 5] = (vec3){5.0f, -1.0f};

	ctx->colors[ctx->vertexCount + 0] = 0xff00a000;
	ctx->colors[ctx->vertexCount + 1] = 0xff00a000;
	ctx->colors[ctx->vertexCount + 2] = 0xff001020;
	ctx->colors[ctx->vertexCount + 3] = 0xff001020;
	ctx->colors[ctx->vertexCount + 4] = 0xff000a10;
	ctx->colors[ctx->vertexCount + 5] = 0xff000a10;

	ctx->vertexCount += 6;

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