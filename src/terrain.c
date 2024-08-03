#if 0

#include "terrain.h"
#include "../shaders/gpu_types.h"
#include "common.h"
#include "vec.h"

#define FNL_IMPL
#include "FastNoiseLite/FastNoiseLite.h"

#include <stdlib.h>
#include <stdio.h>

#define TERRAIN_PATCH_INDEX_COUNT (TERRAIN_PATCH_SIZE * TERRAIN_PATCH_SIZE * 6u)
#define TERRAIN_INDEX_BUFFER_SIZE (TERRAIN_PATCH_INDEX_COUNT * sizeof(uint16_t))
#define TERRAIN_PATCH_HEIGHT_BUFFER_SIZE (TERRAIN_PATCH_VERTEX_COUNT * sizeof(float))
#define TERRAIN_HEIGHT_BUFFER_SIZE (TERRAIN_PATCH_HEIGHT_BUFFER_SIZE * TERRAIN_PATCH_COUNT * TERRAIN_PATCH_COUNT)

#define TERRAIN_PATCH_NORMAL_BUFFER_SIZE (TERRAIN_PATCH_VERTEX_COUNT * sizeof(float) * 4)
#define TERRAIN_NORMAL_BUFFER_SIZE (TERRAIN_PATCH_NORMAL_BUFFER_SIZE * TERRAIN_PATCH_COUNT * TERRAIN_PATCH_COUNT)

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
	VkBuffer		normalBuffer;
	VkDeviceMemory	normalBufferMemory;

	VkBuffer		stagingBuffer;
	void*			stagingBufferData;
	VkBuffer		heightStagingBuffer;
	void*			heightStagingBufferData;
	VkBuffer		normalStagingBuffer;
	void*			normalStagingBufferData;

	int				stagingMemoryCounter;
	int				uploadPatchIndex;
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

	terrain->normalBuffer = CreateBuffer(
		&terrain->normalBufferMemory,
		vulkan,
		TERRAIN_NORMAL_BUFFER_SIZE,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	printf("Terrain buffer size: %.2f MB\n", (TERRAIN_HEIGHT_BUFFER_SIZE+TERRAIN_NORMAL_BUFFER_SIZE)/1024.0f/1024.0f);

	return terrain;
}

void terrain_destroy(terrain_t* terrain)
{
	vulkan_t* vulkan = terrain->vulkan;

	vkDestroyBuffer(vulkan->device, terrain->indexBuffer, NULL);
	vkFreeMemory(vulkan->device, terrain->indexBufferMemory, NULL);
	vkDestroyBuffer(vulkan->device, terrain->heightBuffer, NULL);
	vkFreeMemory(vulkan->device, terrain->heightBufferMemory, NULL);
	vkDestroyBuffer(vulkan->device, terrain->normalBuffer, NULL);
	vkFreeMemory(vulkan->device, terrain->normalBufferMemory, NULL);

	vkDestroyBuffer(vulkan->device, terrain->stagingBuffer, NULL);
	vkDestroyBuffer(vulkan->device, terrain->heightStagingBuffer, NULL);
	vkDestroyBuffer(vulkan->device, terrain->normalStagingBuffer, NULL);

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
		TERRAIN_PATCH_HEIGHT_BUFFER_SIZE,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		"TerrainHeight");

	PushStagingBufferAllocation(
		allocator,
		&terrain->normalStagingBuffer,
		&terrain->normalStagingBufferData,
		TERRAIN_PATCH_NORMAL_BUFFER_SIZE,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		"TerrainNormals");
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

static vec3 fnlGetNoiseNormal2D(fnl_state* noise, FNLfloat x, FNLfloat y)
{
	const float xn = fnlGetNoise2D(noise, x - 1, y);
	const float xp = fnlGetNoise2D(noise, x + 1, y);
	const float yn = fnlGetNoise2D(noise, x, y - 1);
	const float yp = fnlGetNoise2D(noise, x, y + 1);
	
	vec3 n = {
		xn - xp, 
		0.3f, 
		yn - yp,
	};
	return vec3_normalize(n);
}

static void terrain_fill_height_buffer(float* height, vec4* normal, int ox, int oy)
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
			const int gx = x + (ox * TERRAIN_PATCH_SIZE);
			const int gy = y + (oy * TERRAIN_PATCH_SIZE);

			height[index] = fnlGetNoise2D(&noise, gx, gy) * 10.0f;

			const vec3 n = fnlGetNoiseNormal2D(&noise, gx, gy);
			//normal[index] = fnlGetNoiseNormal2D(&noise, gx, gy);
			normal[index] = (vec4){n.x, n.y, n.z};

			++index;
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
			if (terrain->stagingMemoryCounter++ < FRAME_COUNT)
			{
				break;
			}

			terrain->stagingMemoryCounter = 0;

			const int x = terrain->uploadPatchIndex % TERRAIN_PATCH_COUNT;
			const int y = (terrain->uploadPatchIndex / TERRAIN_PATCH_COUNT) % TERRAIN_PATCH_COUNT;

			terrain_fill_height_buffer((float*)terrain->heightStagingBufferData, (vec4*)terrain->normalStagingBufferData, x, y);

			PushStagingMemoryFlush(rc->stagingMemory, terrain->heightStagingBufferData, TERRAIN_PATCH_HEIGHT_BUFFER_SIZE);
			PushStagingMemoryFlush(rc->stagingMemory, terrain->normalStagingBufferData, TERRAIN_PATCH_NORMAL_BUFFER_SIZE);
			
			{
				const VkBufferCopy copyRegion = {
					.size = TERRAIN_PATCH_HEIGHT_BUFFER_SIZE,
					.dstOffset = terrain->uploadPatchIndex * TERRAIN_PATCH_HEIGHT_BUFFER_SIZE,
				};
				vkCmdCopyBuffer(cb, terrain->heightStagingBuffer, terrain->heightBuffer, 1, &copyRegion);
			}
			{
				const VkBufferCopy copyRegion = {
					.size = TERRAIN_PATCH_NORMAL_BUFFER_SIZE,
					.dstOffset = terrain->uploadPatchIndex * TERRAIN_PATCH_NORMAL_BUFFER_SIZE,
				};
				vkCmdCopyBuffer(cb, terrain->normalStagingBuffer, terrain->normalBuffer, 1, &copyRegion);
			}

			++terrain->uploadPatchIndex;
			if (terrain->uploadPatchIndex < (TERRAIN_PATCH_COUNT*TERRAIN_PATCH_COUNT))
			{
				break;
			}

			terrain->state = TERRAIN_STATE_DONE;
			break;
		}
	}
}

bool terrain_get_info(terrain_info_t* info, const terrain_t* terrain)
{
	//if (terrain->state != TERRAIN_STATE_DONE)
	if (terrain->state == TERRAIN_STATE_UPLOAD_INDEX_DATA)
	{
		return false;
	}
	
	info->indexBuffer	= terrain->indexBuffer;
	info->indexCount	= TERRAIN_PATCH_INDEX_COUNT;
	info->heightBuffer	= terrain->heightBuffer;
	info->normalBuffer	= terrain->normalBuffer;
	return true;
}

#endif