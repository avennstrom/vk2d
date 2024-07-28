#pragma once

#include "common.h"
#include "vulkan.h"
#include "shaders.h"
#include "descriptors.h"
#include "types.h"
#include "staging_memory.h"

typedef struct debug_vertex
{
	float x;
	float y;
	float z;
	uint32_t color;
} debug_vertex_t;

typedef struct debug_renderer_frame {
	VkBuffer		vertexBuffer;
	debug_vertex_t*	vertices;
} debug_renderer_frame_t;

typedef struct debug_renderer
{
	debug_renderer_frame_t	frames[FRAME_COUNT];
	
	VkDescriptorSetLayout	descriptorSetLayout;
	VkPipelineLayout		pipelineLayout;
	VkPipeline				pointPipeline;
	VkPipeline				linePipeline;
	VkPipeline				trianglePipeline;
	
	size_t					maxPoints;
	size_t					maxLines;
	size_t					maxTriangles;

	// draw state
	debug_vertex_t*			pointVertices;
	size_t					pointCount;
	debug_vertex_t*			lineVertices;
	size_t					lineCount;
	debug_vertex_t*			triangleVertices;
	size_t					triangleCount;
} debug_renderer_t;

typedef struct debug_renderer_config
{
	size_t	maxPoints;
	size_t	maxLines;
	size_t	maxTriangles;
} debug_renderer_config_t;

int debug_renderer_create(debug_renderer_t* debugRenderer, vulkan_t* vulkan, const debug_renderer_config_t* config);
void debug_renderer_destroy(debug_renderer_t* debugRenderer, vulkan_t* vulkan);

int AllocateDebugRendererStagingMemory(staging_memory_allocator_t* allocator, debug_renderer_t* debugRenderer);
void FlushDebugRenderer(
	VkCommandBuffer cb,
	staging_memory_context_t* stagingMemory,
	debug_renderer_t* debugRenderer,
	descriptor_allocator_t* dsalloc,
	VkDescriptorBufferInfo frameUniformBuffer,
	uint32_t frameIndex);

void MakeCurrentDebugRenderer(debug_renderer_t* debugRenderer);
void DrawDebugPoint(debug_vertex_t vertex);
void DrawDebugLine(debug_vertex_t v0, debug_vertex_t v1);
void DrawDebugTriangle(debug_vertex_t v0, debug_vertex_t v1, debug_vertex_t v2);
void DrawDebugBox(vec3 a, vec3 b, uint32_t color);
void DrawDebugCross(vec3 p, float size, uint32_t color);