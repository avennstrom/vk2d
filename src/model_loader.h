#pragma once

#include "staging_memory.h"
#include "render_context.h"

#include <stdbool.h>

typedef struct model_loader model_loader_t;

typedef struct model_handle {
	uint32_t index;
} model_handle_t;

model_loader_t* model_loader_create(vulkan_t* vulkan);
void model_loader_destroy(model_loader_t* modelLoader);

void model_loader_alloc_staging_mem(staging_memory_allocator_t* allocator, model_loader_t* modelLoader);
void model_loader_update(VkCommandBuffer cb, model_loader_t* modelLoader, const render_context_t* rc);

typedef struct model_loader_info {
	VkBuffer		storageBuffer;
	VkDeviceSize	storageBufferSize;
} model_loader_info_t;

void model_loader_get_info(model_loader_info_t* info, const model_loader_t* modelLoader);

typedef struct model_info {
	uint32_t indexCount;
	uint32_t indexOffset;
	uint32_t vertexPositionOffset;
	uint32_t vertexNormalOffset;
} model_info_t;

bool model_loader_get_model_info(model_info_t* info, const model_loader_t* modelLoader, model_handle_t handle);