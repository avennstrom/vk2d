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

int descriptor_set_cache_create(descriptor_set_cache_t* cache, VkDescriptorPool pool, size_t maxEntries);
void descriptor_set_cache_destroy(descriptor_set_cache_t* cache, vulkan_t* vulkan);
void descriptor_set_cache_update(descriptor_set_cache_t* cache, uint64_t frameId);

int descriptor_allocator_create(descriptor_allocator_t* allocator, descriptor_set_cache_t* cache, vulkan_t* vulkan, size_t maxWrites);
void descriptor_allocator_begin(descriptor_allocator_t* allocator, VkDescriptorSetLayout layout, const char* debugName);
void descriptor_allocator_set_uniform_buffer(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorBufferInfo info);
void descriptor_allocator_set_storage_buffer(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorBufferInfo info);
void descriptor_allocator_set_combined_image_sampler(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorImageInfo info);
void descriptor_allocator_set_sampled_image(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorImageInfo info);
void descriptor_allocator_set_sampler(descriptor_allocator_t* allocator, uint32_t binding, VkSampler sampler);
VkDescriptorSet descriptor_allocator_end(descriptor_allocator_t* allocator);
