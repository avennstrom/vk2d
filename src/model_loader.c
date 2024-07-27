#include "model_loader.h"
#include "offset_allocator.h"
#include "file_format.h"
#include "types.h"
#include "mat.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <memory.h>

#define MODEL_LOADER_STAGING_BUFFER_SIZE (64 * 1024 * 1024) // 64 MB
#define MODEL_LOADER_STORAGE_BUFFER_SIZE (256 * 1024 * 1024) // 256 MB
#define MODEL_LOADER_MAX_MODELS (32)
#define MODEL_LOADER_MAX_PARTS_PER_MODEL (8)
#define MODEL_LOADER_MAX_HIERARCHY_SIZE (8)

typedef struct model model_t;

typedef enum model_state
{
	MODEL_STATE_ALLOCATE_STAGING_MEMORY,
	MODEL_STATE_READ_DATA,
	MODEL_STATE_UPLOAD_DATA,
	MODEL_STATE_LOADED,
} model_state_t;

struct model
{
	model_state_t					state;
	model_t*						next;
	size_t							contentOffset;
	FILEFORMAT_model_header_t		header;
	FILEFORMAT_model_part_header_t	partHeaders[MODEL_LOADER_MAX_PARTS_PER_MODEL];
	model_hierarchy_node_t			hierarchy[MODEL_LOADER_MAX_HIERARCHY_SIZE];
	//uint32_t						dataSize;
	offset_allocation_t				stagingAllocation;
	uint32_t						storageAllocation;
};

struct model_loader
{
	vulkan_t*			vulkan;
	content_t*			content;

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

static void model_loader_start_load_models(model_loader_t* modelLoader, const game_resource_t* gameResource);

model_loader_t* model_loader_create(vulkan_t* vulkan, const game_resource_t* gameResource, content_t* content)
{
	int r;
	
	model_loader_t* modelLoader = calloc(1, sizeof(model_loader_t));
	if (modelLoader == NULL)
	{
		return NULL;
	}

	modelLoader->vulkan		= vulkan;
	modelLoader->content	= content;

	r = offset_allocator_create(&modelLoader->stagingAllocator, MODEL_LOADER_STAGING_BUFFER_SIZE, MODEL_LOADER_MAX_MODELS);
	assert(r == 0);

	modelLoader->storageBuffer = CreateBuffer(
		&modelLoader->storageBufferMemory,
		vulkan,
		MODEL_LOADER_STORAGE_BUFFER_SIZE,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	assert(modelLoader->storageBuffer != NULL);

	model_loader_start_load_models(modelLoader, gameResource);

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
			case MODEL_STATE_ALLOCATE_STAGING_MEMORY:
			{
				if (!offset_allocator_alloc(&model->stagingAllocation, &modelLoader->stagingAllocator, model->header.dataSize))
				{
					break;
				}
				
				model->state = MODEL_STATE_READ_DATA;
				break;
			}

			case MODEL_STATE_READ_DATA:
			{
				uint8_t* data = modelLoader->stagingMemory + model->stagingAllocation.offset;
				//const int nread = fread(data, 1, model->header.dataSize, model->file);
				//assert(nread == model->header.dataSize); // :todo:
				content_read(data, modelLoader->content, model->header.contentOffset, model->header.dataSize);
				
				model->state = MODEL_STATE_UPLOAD_DATA;
				break;
			}

			case MODEL_STATE_UPLOAD_DATA:
			{
				VkBufferCopy* copy = &copyRegions[copyCount++];
				copy->srcOffset = model->stagingAllocation.offset;
				copy->dstOffset = model->storageAllocation;
				copy->size = model->header.dataSize;
				
				model->state = MODEL_STATE_LOADED;
				break;
			}
		}
		
		if (model->state == MODEL_STATE_LOADED)
		{
			printf("Model loaded.\n");

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

static void model_loader_start_load_models(model_loader_t* modelLoader, const game_resource_t* gameResource)
{
	assert(gameResource->modelCount < MODEL_LOADER_MAX_MODELS);

	for (size_t i = 0; i < gameResource->modelCount; ++i)
	{
		model_t* model = &modelLoader->models[i];

		const FILEFORMAT_game_resource_model_entry_t* modelEntry = &gameResource->models[i];
		const uint64_t headerOffset = modelEntry->headerOffset;
		const FILEFORMAT_model_header_t* modelHeader = (FILEFORMAT_model_header_t*)(gameResource->mem + headerOffset);
		const FILEFORMAT_model_part_header_t* partHeaders = (FILEFORMAT_model_part_header_t*)(modelHeader + 1);
	
		memcpy(&model->header, modelHeader, sizeof(model->header));
		memcpy(model->partHeaders, partHeaders, sizeof(FILEFORMAT_model_part_header_t) * model->header.partCount);

		for (uint i = 0; i < model->header.partCount; ++i)
		{
			mat4 m = mat_identity();
			m = mat_rotate(m, model->partHeaders[i].rotation);
			m = mat_translate(m, model->partHeaders[i].translation);

			model->hierarchy[i].localTransform	= m;
			model->hierarchy[i].parentIndex		= model->partHeaders[i].parentIndex;
		}

		model->storageAllocation = modelLoader->storageBufferAllocator;
		modelLoader->storageBufferAllocator += model->header.dataSize;

		assert(model->storageAllocation % 4 == 0); // for storage buffer access

		model->next = modelLoader->loadingModels;
		modelLoader->loadingModels = model;
	}

	//model_loader_start_load_model(modelLoader, &modelLoader->models[0], "dat/tank.model");
	//model_loader_start_load_model(modelLoader, &modelLoader->models[0], "dat/colortest.model");
	//model_loader_start_load_model(modelLoader, &modelLoader->models[1], "dat/cube.model");
	//model_loader_start_load_model(modelLoader, &modelLoader->models[2], "dat/sphere.model");
}

void model_loader_get_info(model_loader_info_t* info, const model_loader_t* modelLoader)
{
	info->storageBuffer		= modelLoader->storageBuffer;
	info->storageBufferSize	= MODEL_LOADER_STORAGE_BUFFER_SIZE;
}

static model_part_info_t partInfos[MODEL_LOADER_MAX_PARTS_PER_MODEL];

bool model_loader_get_model_info(model_info_t* info, const model_loader_t* modelLoader, model_handle_t handle)
{
	const model_t* model = &modelLoader->models[handle.index];
	if (model->state != MODEL_STATE_LOADED)
	{
		return false;
	}

	const uint32_t partCount = model->header.partCount;
	const uint32_t dataOffset = model->storageAllocation;
	
	for (int i = 0; i < partCount; ++i)
	{
		model_part_info_t* partInfo = &partInfos[i];
		partInfo->indexCount			= model->partHeaders[i].indexCount;
		partInfo->indexOffset			= (dataOffset + model->partHeaders[i].indexDataOffset) / 2;
		partInfo->vertexPositionOffset	= dataOffset + model->partHeaders[i].vertexPositionDataOffset;
		partInfo->vertexNormalOffset	= dataOffset + model->partHeaders[i].vertexNormalDataOffset;
		partInfo->vertexColorOffset		= dataOffset + model->partHeaders[i].vertexColorDataOffset;
	}

	info->partCount	= partCount;
	info->parts		= partInfos;

	return true;
}

void model_loader_get_model_hierarchy(model_hierarchy_t* hierarchy, const model_loader_t* modelLoader, model_handle_t handle)
{
	*hierarchy = (model_hierarchy_t){};

	const model_t* model = &modelLoader->models[handle.index];
	if (model->state != MODEL_STATE_LOADED)
	{
		return;
	}

	hierarchy->size		= model->header.partCount;
	hierarchy->nodes	= model->hierarchy;
}

void model_hierarchy_resolve(mat4* resolved, const mat4* localTransforms, const model_hierarchy_t* hierarchy)
{
	for (uint i = 0; i < hierarchy->size; ++i)
	{
		mat4 m = localTransforms[i];
		const uint parentIndex = hierarchy->nodes[i].parentIndex;
		if (parentIndex != 0xffffffff)
		{
			m = mat_mul(m, resolved[parentIndex]);
		}

		resolved[i] = mat_mul(hierarchy->nodes[i].localTransform, m);
	}
}