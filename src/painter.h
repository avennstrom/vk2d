#pragma once

#include "vulkan.h"
#include "staging_memory.h"

typedef struct painter painter_t;

painter_t* CreatePainter(
	vulkan_t* vulkan);

int AllocatePainterStagingMemory(
	staging_memory_allocator_t* allocator,
	painter_t* painter);

void TickPainter(
	painter_t* painter,
	uint64_t frameId);

void RenderPaintings(
	VkCommandBuffer cb,
	painter_t* painter,
	staging_memory_context_t* staging,
	uint64_t frameId);

VkDescriptorImageInfo GetPaintingImage(
	painter_t* painter);