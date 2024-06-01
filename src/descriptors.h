#pragma once

#include "vulkan.h"

#include <stddef.h>

typedef struct descriptor_set_cache {
	VkDescriptorPool		pool;
	size_t					capacity;
	size_t					count;
	uint64_t				currentFrame;
	uint64_t*				frameIds;
	VkDescriptorSetLayout*	layouts;
	VkDescriptorSet*		sets;
} descriptor_set_cache_t;

typedef struct descriptor_allocator {
	descriptor_set_cache_t*	cache;
	vulkan_t*				vulkan;
	VkDescriptorSet			currentSet;
	VkWriteDescriptorSet*	writes;
	size_t					writeCount;
	VkDescriptorImageInfo*	imageInfos;
	size_t					imageCount;
	VkDescriptorBufferInfo*	bufferInfos;
	size_t					bufferCount;
} descriptor_allocator_t;

int CreateDescriptorSetCache(descriptor_set_cache_t* cache, VkDescriptorPool pool, size_t maxEntries);
void UpdateDescriptorSetCache(descriptor_set_cache_t* cache, uint64_t frameId);

int CreateDescriptorAllocator(descriptor_allocator_t* allocator, descriptor_set_cache_t* cache, vulkan_t* vulkan, size_t maxWrites);

void StartBindingDescriptors(descriptor_allocator_t* allocator, VkDescriptorSetLayout layout, const char* debugName);
void BindUniformBuffer(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorBufferInfo info);
void BindStorageBuffer(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorBufferInfo info);
void BindCombinedImageSampler(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorImageInfo info);
VkDescriptorSet FinishBindingDescriptors(descriptor_allocator_t* allocator);
