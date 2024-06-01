#pragma once

#include "vulkan.h"
#include "debug_renderer.h"
#include "descriptors.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VOXEL_SIZE_X 3.5f
#define VOXEL_SIZE_Y 5.0f
#define VOXEL_SIZE_Z 3.5f
#define CHUNK_SIZE_X 32
#define CHUNK_SIZE_Y 8
#define CHUNK_SIZE_Z 32

typedef struct world_vertex {
	ushort3 pos;
	uint16_t normal;
	uint32_t flags0;
	uint32_t flags1;
} world_vertex_t;
_Static_assert(sizeof(world_vertex_t) == 16, "");

typedef struct worldgen worldgen_t;

typedef struct worldgen_info {
	VkPipelineLayout	pipelineLayout;
	VkPipeline			pipeline;
	VkPipeline			shadowPipeline;
	VkBuffer			vertexBuffer;
	size_t				vertexCount;
} worldgen_info_t;

worldgen_t* CreateWorldgen(
	vulkan_t* vulkan);

void DestroyWorldgen(
	worldgen_t* worldgen);

void GetWorldgenInfo(
	worldgen_info_t* info,
	worldgen_t* worldgen);

int AllocateWorldgenStagingMemory(
	staging_memory_allocator_t* allocator,
	worldgen_t* worldgen);

int TickWorldgen(
	worldgen_t* worldgen);

void DebugVisualizeWorldgen(
	worldgen_t* worldgen);

VkDescriptorSet CreateWorldDescriptorSet(
	worldgen_t* worldgen,
	descriptor_allocator_t* dsalloc,
	VkDescriptorBufferInfo frameUniformBuffer,
	VkDescriptorBufferInfo pointLightBuffer,
	VkDescriptorImageInfo paintingImage,
	VkDescriptorImageInfo pointShadowAtlas);

void DrawWorld(
	VkCommandBuffer cb,
	worldgen_t* worldgen,
	VkDescriptorSet descriptorSet);

int GetWorldConnectivity(
	uint8_t connectivity[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z], 
	worldgen_t* worldgen);

bool IsChunkOutOfBounds(
	short3 pos);