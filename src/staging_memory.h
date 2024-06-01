#pragma once

#include "vulkan.h"

typedef struct staging_memory_allocator
{
	vulkan_t*		vulkan;

	VkDeviceSize	allocSize;
	size_t			count;
	uint32_t		memoryTypeBits;

	VkBuffer		handles[64];
	const char*		names[64];
	VkDeviceSize	offsets[64];
	VkDeviceSize	sizes[64];
	void**			maps[64];
} staging_memory_allocator_t;

typedef struct staging_memory_allocation
{
	VkDeviceMemory	memory;
	void*			mappedMemory;
} staging_memory_allocation_t;

typedef struct staging_memory_context {
	staging_memory_allocation_t	allocation;
	size_t						flushCount;
	VkMappedMemoryRange			flushRanges[64];
} staging_memory_context_t;

void ResetStagingMemoryAllocator(
	staging_memory_allocator_t* allocator,
	vulkan_t* vulkan);

void PushStagingBufferAllocation(
	staging_memory_allocator_t* allocator,
	VkBuffer* buffer,
	void** mappedAddress,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	const char* debugName);

VkResult FinalizeStagingMemoryAllocator(
	staging_memory_allocation_t* allocation,
	staging_memory_allocator_t* allocator);

void ResetStagingMemoryContext(
	staging_memory_context_t* ctx,
	const staging_memory_allocation_t* allocation);

void PushStagingMemoryFlush(
	staging_memory_context_t* ctx,
	void* ptr,
	size_t size);

VkResult FlushStagingMemory(
	staging_memory_context_t* ctx,
	vulkan_t* vulkan);