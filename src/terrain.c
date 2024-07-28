#include "terrain.h"
#include "../shaders/gpu_types.h"

#define FNL_IMPL
#include "FastNoiseLite/FastNoiseLite.h"

#include <stdlib.h>

#define TERRAIN_PATCH_INDEX_COUNT (TERRAIN_PATCH_SIZE * TERRAIN_PATCH_SIZE * 6u)
#define TERRAIN_PATCH_VERTEX_COUNT (TERRAIN_PATCH_INDEX_STRIDE * TERRAIN_PATCH_INDEX_STRIDE)
#define TERRAIN_INDEX_BUFFER_SIZE (TERRAIN_PATCH_INDEX_COUNT * sizeof(uint16_t))
#define TERRAIN_HEIGHT_BUFFER_SIZE (TERRAIN_PATCH_VERTEX_COUNT * sizeof(float))

_Static_assert(TERRAIN_PATCH_VERTEX_COUNT <= 0xffff, "Too many vertices in terrain patch for uint16 index buffer");

typedef enum terrain_state
{
	TERRAIN_STATE_UPLOAD_INDEX_DATA,
	TERRAIN_STATE_UPLOAD_VERTEX_DATA,
	TERRAIN_STATE_DONE,
} terrain_state_t;

typedef struct terrain
{
	vulkan_t*		vulkan;
	terrain_state_t	state;

	VkBuffer		indexBuffer;
	VkDeviceMemory	indexBufferMemory;
	VkBuffer		heightBuffer;
	VkDeviceMemory	heightBufferMemory;

	VkBuffer		stagingBuffer;
	void*			stagingBufferData;
	VkBuffer		heightStagingBuffer;
	void*			heightStagingBufferData;
} terrain_t;

terrain_t* terrain_create(vulkan_t* vulkan)
{
	terrain_t* terrain = calloc(1, sizeof(terrain_t));
	if (terrain == NULL)
	{
		return NULL;
	}

	terrain->vulkan = vulkan;
	
	terrain->indexBuffer = CreateBuffer(
		&terrain->indexBufferMemory,
		vulkan,
		TERRAIN_INDEX_BUFFER_SIZE,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	terrain->heightBuffer = CreateBuffer(
		&terrain->heightBufferMemory,
		vulkan,
		TERRAIN_HEIGHT_BUFFER_SIZE,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	return terrain;
}

void terrain_destroy(terrain_t* terrain)
{
	vulkan_t* vulkan = terrain->vulkan;

	vkDestroyBuffer(vulkan->device, terrain->indexBuffer, NULL);
	vkFreeMemory(vulkan->device, terrain->indexBufferMemory, NULL);
	vkDestroyBuffer(vulkan->device, terrain->heightBuffer, NULL);
	vkFreeMemory(vulkan->device, terrain->heightBufferMemory, NULL);

	vkDestroyBuffer(vulkan->device, terrain->stagingBuffer, NULL);
	vkDestroyBuffer(vulkan->device, terrain->heightStagingBuffer, NULL);

	free(terrain);
}

void terrain_alloc_staging_mem(staging_memory_allocator_t* allocator, terrain_t* terrain)
{
	PushStagingBufferAllocation(
		allocator,
		&terrain->stagingBuffer,
		&terrain->stagingBufferData,
		TERRAIN_INDEX_BUFFER_SIZE,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		"TerrainIndices");
	
	PushStagingBufferAllocation(
		allocator,
		&terrain->heightStagingBuffer,
		&terrain->heightStagingBufferData,
		TERRAIN_HEIGHT_BUFFER_SIZE,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		"TerrainHeight");
}

static void terrain_fill_index_buffer(uint16_t* indices)
{
	for (uint z = 0; z < TERRAIN_PATCH_SIZE; ++z)
	{
		for (uint x = 0; x < TERRAIN_PATCH_SIZE; ++x)
		{
			const uint i = x + (z * TERRAIN_PATCH_INDEX_STRIDE);

			indices[0] = i;
			indices[1] = i + 1;
			indices[2] = i + 1 + TERRAIN_PATCH_INDEX_STRIDE;

			indices[3] = i + 1 + TERRAIN_PATCH_INDEX_STRIDE;
			indices[4] = i;
			indices[5] = i + TERRAIN_PATCH_INDEX_STRIDE;
			
			indices += 6;
		}
	}
}

static void terrain_fill_height_buffer(float* height)
{
	fnl_state noise = fnlCreateState();
	noise.noise_type = FNL_NOISE_CELLULAR;
	noise.octaves = 10;
	noise.frequency = 0.05f;
	noise.gain = 0.8f;
	noise.lacunarity = 5.0f;

	int index = 0;
	for (int y = 0; y < TERRAIN_PATCH_INDEX_STRIDE; y++)
	{
		for (int x = 0; x < TERRAIN_PATCH_INDEX_STRIDE; x++) 
		{
			float n = fnlGetNoise2D(&noise, x, y) * 10.0f;
			height[index++] = n;
		}
	}
}

void terrain_update(VkCommandBuffer cb, terrain_t* terrain, const render_context_t* rc)
{
	switch (terrain->state)
	{
		case TERRAIN_STATE_UPLOAD_INDEX_DATA:
		{
			terrain_fill_index_buffer((uint16_t*)terrain->stagingBufferData);
			PushStagingMemoryFlush(rc->stagingMemory, terrain->stagingBufferData, TERRAIN_INDEX_BUFFER_SIZE);
			
			const VkBufferCopy copyRegion = {
				.size = TERRAIN_INDEX_BUFFER_SIZE,
			};
			vkCmdCopyBuffer(cb, terrain->stagingBuffer, terrain->indexBuffer, 1, &copyRegion);
			
			terrain->state = TERRAIN_STATE_UPLOAD_VERTEX_DATA;
			break;
		}

		case TERRAIN_STATE_UPLOAD_VERTEX_DATA:
		{
			terrain_fill_height_buffer((float*)terrain->heightStagingBufferData);
			PushStagingMemoryFlush(rc->stagingMemory, terrain->heightStagingBufferData, TERRAIN_HEIGHT_BUFFER_SIZE);
			
			const VkBufferCopy copyRegion = {
				.size = TERRAIN_HEIGHT_BUFFER_SIZE,
			};
			vkCmdCopyBuffer(cb, terrain->heightStagingBuffer, terrain->heightBuffer, 1, &copyRegion);

			terrain->state = TERRAIN_STATE_DONE;
			break;
		}
	}
}

bool terrain_get_info(terrain_info_t* info, const terrain_t* terrain)
{
	if (terrain->state != TERRAIN_STATE_DONE)
	{
		return false;
	}
	
	info->indexBuffer	= terrain->indexBuffer;
	info->indexCount	= TERRAIN_PATCH_INDEX_COUNT;
	info->heightBuffer	= terrain->heightBuffer;
	return true;
}