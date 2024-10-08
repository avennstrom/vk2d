#pragma once

typedef struct vulkan vulkan_t;
typedef struct staging_memory_context staging_memory_context_t;
typedef struct descriptor_allocator descriptor_allocator_t;

typedef struct render_context
{
	uint32_t					frameIndex;
	vulkan_t*					vulkan;
	staging_memory_context_t*	stagingMemory;
	descriptor_allocator_t*		dsalloc;
} render_context_t;

