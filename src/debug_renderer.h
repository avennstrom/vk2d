#pragma once

#include "common.h"
#include "vulkan.h"
#include "shaders.h"
#include "descriptors.h"
#include "types.h"
#include "staging_memory.h"

typedef struct debug_renderer debug_renderer_t;

typedef struct debug_vertex
{
	float x;
	float y;
	float z;
	uint32_t color;
} debug_vertex_t;

typedef struct debug_renderer_config
{
	size_t	maxPoints;
	size_t	maxLines;
	size_t	maxTriangles;
} debug_renderer_config_t;

typedef enum debug_renderer_buffer_id
{
	DEBUG_RENDERER_BUFFER_TICK,
	DEBUG_RENDERER_BUFFER_FRAME,
	DEBUG_RENDERER_BUFFER_COUNT,
} debug_renderer_buffer_id_t;

typedef enum debug_renderer_view_id
{
	DEBUG_RENDERER_VIEW_2D,
	DEBUG_RENDERER_VIEW_3D,
	DEBUG_RENDERER_VIEW_COUNT,
} debug_renderer_view_id_t;

debug_renderer_t* debug_renderer_create(vulkan_t* vulkan, const debug_renderer_config_t* config);
void debug_renderer_destroy(debug_renderer_t* debugRenderer, vulkan_t* vulkan);

int AllocateDebugRendererStagingMemory(staging_memory_allocator_t* allocator, debug_renderer_t* debugRenderer);

void debug_renderer_set_current_buffer(debug_renderer_t* debugRenderer, debug_renderer_buffer_id_t id);
void debug_renderer_clear_buffer(debug_renderer_t* debugRenderer, debug_renderer_buffer_id_t id);

void debug_renderer_flush(
	VkCommandBuffer cb,
	staging_memory_context_t* stagingMemory,
	debug_renderer_t* debugRenderer,
	descriptor_allocator_t* dsalloc,
	uint32_t frameIndex,
	debug_renderer_view_id_t viewId,
	mat4 viewProjectionMatrix);

// 2d
void DrawDebugPoint2D(debug_vertex_t vertex);

// 3d
void DrawDebugPoint(debug_vertex_t vertex);
void DrawDebugLine(debug_vertex_t v0, debug_vertex_t v1);
void DrawDebugTriangle(debug_vertex_t v0, debug_vertex_t v1, debug_vertex_t v2);
void DrawDebugBox(vec3 a, vec3 b, uint32_t color);
void DrawDebugCross(vec3 p, float size, uint32_t color);