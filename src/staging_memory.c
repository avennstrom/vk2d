#include "staging_memory.h"
#include "util.h"

#include <stdio.h>
#include <assert.h>

void ResetStagingMemoryAllocator(
	staging_memory_allocator_t* allocator,
	vulkan_t* vulkan)
{
	allocator->vulkan = vulkan;
	allocator->allocSize = 0;
	allocator->count = 0;
	allocator->memoryTypeBits = 0;
}

void PushStagingBufferAllocation(
	staging_memory_allocator_t* allocator, 
	VkBuffer* buffer, 
	void** mappedMemory, 
	VkDeviceSize size, 
	VkBufferUsageFlags usage,
	const char* debugName)
{
	VkResult r;

	const VkBufferCreateInfo bufferInfo = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
	};

	r = vkCreateBuffer(allocator->vulkan->device, &bufferInfo, NULL, buffer);
	assert(r == VK_SUCCESS);

	SetBufferName(allocator->vulkan, *buffer, debugName);

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(allocator->vulkan->device, *buffer, &memoryRequirements);

	const VkDeviceSize alignment = memoryRequirements.alignment;
	const VkDeviceSize offset = (allocator->allocSize + alignment - 1) / alignment * alignment;

	allocator->memoryTypeBits |= memoryRequirements.memoryTypeBits;
	allocator->allocSize = offset + memoryRequirements.size;

	allocator->handles[allocator->count] = *buffer;
	allocator->names[allocator->count] = debugName;
	allocator->offsets[allocator->count] = offset;
	allocator->sizes[allocator->count] = memoryRequirements.size;
	allocator->maps[allocator->count] = mappedMemory;
	++allocator->count;
}

VkResult FinalizeStagingMemoryAllocator(
	staging_memory_allocation_t* allocation,
	staging_memory_allocator_t* allocator)
{
	VkResult r;

	uint32_t memoryTypeIndex = FindMemoryType(allocator->vulkan, allocator->memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	assert(memoryTypeIndex != 0xffffffffu);

	const VkMemoryAllocateInfo memoryAllocateInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = allocator->allocSize,
		.memoryTypeIndex = memoryTypeIndex,
	};

	r = vkAllocateMemory(allocator->vulkan->device, &memoryAllocateInfo, NULL, &allocation->memory);
	if (r != VK_SUCCESS) {
		return r;
	}

	printf("Staging memory: %.2f MB\n", allocator->allocSize/1024.0f/1024.0f);

	vkMapMemory(allocator->vulkan->device, allocation->memory, 0, VK_WHOLE_SIZE, 0, &allocation->mappedMemory);

	for (size_t i = 0; i < allocator->count; ++i)
	{
		const VkBuffer buffer = allocator->handles[i];
		const VkDeviceSize offset = allocator->offsets[i];
		
		*allocator->maps[i] = (uint8_t*)allocation->mappedMemory + offset;

		vkBindBufferMemory(allocator->vulkan->device, buffer, allocation->memory, offset);

		printf("  %08x:%08x %10.2f kB (%s)\n", offset, allocator->sizes[i], allocator->sizes[i]/1024.0f, allocator->names[i]);
	}
}

void ResetStagingMemoryContext(
	staging_memory_context_t* ctx,
	const staging_memory_allocation_t* allocation)
{
	ctx->allocation = *allocation;
	ctx->flushCount = 0;
}

void PushStagingMemoryFlush(
	staging_memory_context_t* ctx,
	void* ptr,
	size_t size)
{
	assert(ctx->flushCount < countof(ctx->flushRanges));
	assert((uintptr_t)ptr >= (uintptr_t)ctx->allocation.mappedMemory);
	const ptrdiff_t offset = (uintptr_t)ptr - (uintptr_t)ctx->allocation.mappedMemory;

	size = alignUp(size, 0x40); //:todo:

	ctx->flushRanges[ctx->flushCount++] = (VkMappedMemoryRange){
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = ctx->allocation.memory,
		.offset = offset,
		.size = size,
	};

	//printf("flush [%p:%x]\n", ptr, size);
}

VkResult FlushStagingMemory(
	staging_memory_context_t* ctx,
	vulkan_t* vulkan)
{
	if (ctx->flushCount == 0) {
		return VK_SUCCESS;
	}

	return vkFlushMappedMemoryRanges(vulkan->device, ctx->flushCount, ctx->flushRanges);
}