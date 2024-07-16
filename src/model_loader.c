#include "model_loader.h"
#include "offset_allocator.h"
#include "file_format.h"
#include "types.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <memory.h>

#define MODEL_LOADER_STAGING_BUFFER_SIZE (64 * 1024 * 1024) // 64 MB
#define MODEL_LOADER_STORAGE_BUFFER_SIZE (256 * 1024 * 1024) // 256 MB
#define MODEL_LOADER_MAX_MODELS (32)

typedef struct model model_t;

typedef enum model_state
{
	MODEL_STATE_READ_HEADER = 0,
	MODEL_STATE_ALLOCATE_STORAGE_MEMORY,
	MODEL_STATE_ALLOCATE_STAGING_MEMORY,
	MODEL_STATE_READ_DATA,
	MODEL_STATE_UPLOAD_DATA,
	MODEL_STATE_LOADED,
} model_state_t;

struct model
{
	model_state_t				state;
	model_t*					next;
	FILE*						file;
	FILEFORMAT_model_header_t	header;
	offset_allocation_t			stagingAllocation;
	uint32_t					storageAllocation;
};

struct model_loader
{
	vulkan_t*			vulkan;

	VkBuffer			stagingBuffer;
	uint8_t*			stagingMemory;
	offset_allocator_t	stagingAllocator;
	model_t*			loadingModels;
	model_t				models[MODEL_LOADER_MAX_MODELS];

	VkDeviceMemory		storageBufferMemory;
	VkBuffer			storageBuffer;
	uint32_t			storageBufferAllocator;
	
	VkDeviceMemory		modelBufferMemory;
	VkBuffer			modelBuffer;
};

static void model_loader_start_load_models(model_loader_t* modelLoader);

model_loader_t* model_loader_create(vulkan_t* vulkan)
{
	int r;
	
	model_loader_t* modelLoader = calloc(1, sizeof(model_loader_t));
	if (modelLoader == NULL)
	{
		return NULL;
	}

	modelLoader->vulkan = vulkan;

	r = offset_allocator_create(&modelLoader->stagingAllocator, MODEL_LOADER_STAGING_BUFFER_SIZE, MODEL_LOADER_MAX_MODELS);
	assert(r == 0);

	modelLoader->storageBuffer = CreateBuffer(
		&modelLoader->storageBufferMemory,
		vulkan,
		MODEL_LOADER_STORAGE_BUFFER_SIZE,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	assert(modelLoader->storageBuffer != NULL);

	model_loader_start_load_models(modelLoader);

	return modelLoader;
}

void model_loader_destroy(model_loader_t* modelLoader)
{
	offset_allocator_destroy(&modelLoader->stagingAllocator);
	
	vulkan_t* vulkan = modelLoader->vulkan;
	vkDestroyBuffer(vulkan->device, modelLoader->stagingBuffer, NULL);
	vkDestroyBuffer(vulkan->device, modelLoader->storageBuffer, NULL);
	vkFreeMemory(vulkan->device, modelLoader->storageBufferMemory, NULL);
}

void model_loader_alloc_staging_mem(staging_memory_allocator_t* allocator, model_loader_t* modelLoader)
{
	PushStagingBufferAllocation(
		allocator,
		&modelLoader->stagingBuffer,
		(void**)&modelLoader->stagingMemory,
		MODEL_LOADER_STAGING_BUFFER_SIZE,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		"ModelLoader");
}

size_t model_loader_CalculateIndexDataSize(const FILEFORMAT_model_header_t* header)
{
	return header->indexCount * sizeof(uint16_t);
}

size_t model_loader_CalculateVertexDataSize(const FILEFORMAT_model_header_t* header)
{
	return header->vertexCount * 6*sizeof(float);
}

size_t model_loader_CalculateModelDataSize(const FILEFORMAT_model_header_t* header)
{
	return model_loader_CalculateIndexDataSize(header) + model_loader_CalculateVertexDataSize(header);
}

void model_loader_update(VkCommandBuffer cb, model_loader_t* modelLoader, const render_context_t* rc)
{
	VkBufferCopy copyRegions[MODEL_LOADER_MAX_MODELS];
	uint32_t copyCount = 0;

	model_t** prevnext = &modelLoader->loadingModels;
	for (model_t* model = modelLoader->loadingModels; model != NULL;)
	{
		assert(model->state != MODEL_STATE_LOADED);

		switch (model->state)
		{
			case MODEL_STATE_READ_HEADER:
			{
				const int nread = fread(&model->header, sizeof(FILEFORMAT_model_header_t), 1, model->file);
				assert(nread == 1);

				model->state = MODEL_STATE_ALLOCATE_STORAGE_MEMORY;
				break;
			}

			case MODEL_STATE_ALLOCATE_STORAGE_MEMORY:
			{
				const size_t dataSize = model_loader_CalculateModelDataSize(&model->header);

				model->storageAllocation = modelLoader->storageBufferAllocator;
				modelLoader->storageBufferAllocator += dataSize;

				model->state = MODEL_STATE_ALLOCATE_STAGING_MEMORY;
				break;
			}

			case MODEL_STATE_ALLOCATE_STAGING_MEMORY:
			{
				const size_t dataSize = model_loader_CalculateModelDataSize(&model->header);
				if (!offset_allocator_alloc(&model->stagingAllocation, &modelLoader->stagingAllocator, dataSize))
				{
					break;
				}
				
				model->state = MODEL_STATE_READ_DATA;
				break;
			}

			case MODEL_STATE_READ_DATA:
			{
				uint8_t* data = modelLoader->stagingMemory + model->stagingAllocation.offset;
				const size_t dataSize = model_loader_CalculateModelDataSize(&model->header);
				const int nread = fread(data, 1, dataSize, model->file);
				assert(nread == dataSize); // :todo:
				
				model->state = MODEL_STATE_UPLOAD_DATA;
				break;
			}

			case MODEL_STATE_UPLOAD_DATA:
			{
				const size_t dataSize = model_loader_CalculateModelDataSize(&model->header);

				VkBufferCopy* copy = &copyRegions[copyCount++];
				copy->srcOffset = model->stagingAllocation.offset;
				copy->dstOffset = model->storageAllocation;
				copy->size = dataSize;
				
				model->state = MODEL_STATE_LOADED;
				break;
			}
		}
		
		if (model->state == MODEL_STATE_LOADED)
		{
			*prevnext = model->next;
			model->next = NULL;
			model = *prevnext;
		}
		else
		{
			prevnext = &model->next;
			model = model->next;
		}
	}

	if (copyCount > 0)
	{
		vkCmdCopyBuffer(cb, modelLoader->stagingBuffer, modelLoader->storageBuffer, copyCount, copyRegions);

		for (uint i = 0; i < copyCount; ++i)
		{
			PushStagingMemoryFlush(rc->stagingMemory, modelLoader->stagingMemory + copyRegions->srcOffset, copyRegions->size);
		}
	}
}

static void model_loader_StartLoadModel(model_loader_t* modelLoader, model_t* model, const char* filename)
{
	model->file	= fopen(filename, "rb");
	if (model->file == NULL)
	{
		fprintf(stderr, "Failed to open model '%s'\n", filename);
		return;
	}

	model->next = modelLoader->loadingModels;
	modelLoader->loadingModels = model;
}

static void model_loader_start_load_models(model_loader_t* modelLoader)
{
	model_loader_StartLoadModel(modelLoader, &modelLoader->models[0], "dat/cube.model");
	model_loader_StartLoadModel(modelLoader, &modelLoader->models[1], "dat/sphere.model");
}

void model_loader_get_info(model_loader_info_t* info, const model_loader_t* modelLoader)
{
	info->storageBuffer		= modelLoader->storageBuffer;
	info->storageBufferSize	= MODEL_LOADER_STORAGE_BUFFER_SIZE;
}

bool model_loader_get_model_info(model_info_t* info, const model_loader_t* modelLoader, model_handle_t handle)
{
	const model_t* model = &modelLoader->models[handle.index];
	if (model->state != MODEL_STATE_LOADED)
	{
		return false;
	}

	assert(model->stagingAllocation.offset % 2 == 0);

	info->indexCount			= model->header.indexCount;
	info->indexOffset			= model->stagingAllocation.offset / 2;
	info->vertexPositionOffset	= model->stagingAllocation.offset + info->indexCount * sizeof(uint16_t);
	info->vertexNormalOffset	= info->vertexPositionOffset + model->header.vertexCount * sizeof(vec3);

	return true;
}