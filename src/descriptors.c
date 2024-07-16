#include "descriptors.h"
#include "common.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

int descriptor_set_cache_create(descriptor_set_cache_t* cache, VkDescriptorPool pool, size_t maxEntries)
{
	memset(cache, 0, sizeof(descriptor_set_cache_t));
	cache->pool = pool;
	cache->capacity = maxEntries;

	cache->frameIds = malloc(maxEntries * sizeof(uint32_t));
	cache->layouts = malloc(maxEntries * sizeof(VkDescriptorSetLayout));
	cache->sets = malloc(maxEntries * sizeof(VkDescriptorSet));

	return 0;
}

void descriptor_set_cache_destroy(descriptor_set_cache_t* cache, vulkan_t* vulkan)
{
	//vkFreeDescriptorSets(vulkan->device, cache->pool, cache->count, cache->sets);

	free(cache->frameIds);
	free(cache->layouts);
	free(cache->sets);
}

void descriptor_set_cache_update(descriptor_set_cache_t* cache, uint64_t frameId)
{
	cache->currentFrame = frameId;
}

int descriptor_allocator_create(descriptor_allocator_t* allocator, descriptor_set_cache_t* cache, vulkan_t* vulkan, size_t maxWrites)
{
	memset(allocator, 0, sizeof(descriptor_allocator_t));
	allocator->cache = cache;
	allocator->vulkan = vulkan;

	allocator->writes = malloc(maxWrites * sizeof(VkWriteDescriptorSet));
	allocator->imageInfos = malloc(maxWrites * sizeof(VkDescriptorImageInfo));
	allocator->bufferInfos = malloc(maxWrites * sizeof(VkDescriptorBufferInfo));

	return 0;
}

static VkDescriptorSet AllocateDescriptorSet(descriptor_set_cache_t* cache, vulkan_t* vulkan, VkDescriptorSetLayout layout, const char* debugName)
{
	for (size_t i = 0; i < cache->count; ++i) {
		if (cache->layouts[i] != layout) {
			continue;
		}

		const uint64_t age = cache->currentFrame - cache->frameIds[i];
		if (age < FRAME_COUNT) {
			continue;
		}

		cache->frameIds[i] = cache->currentFrame;
		return cache->sets[i];
	}
	
	const VkDescriptorSetAllocateInfo allocateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = cache->pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	VkDescriptorSet set;
	if (vkAllocateDescriptorSets(vulkan->device, &allocateInfo, &set) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	SetDescriptorSetName(vulkan, set, debugName);

	cache->frameIds[cache->count] = cache->currentFrame;
	cache->layouts[cache->count] = layout;
	cache->sets[cache->count] = set;
	++cache->count;

	return set;
}

void descriptor_allocator_begin(
	descriptor_allocator_t* allocator,
	VkDescriptorSetLayout layout,
	const char* debugName)
{
	allocator->writeCount = 0;
	allocator->imageCount = 0;
	allocator->bufferCount = 0;
	
	allocator->currentSet = AllocateDescriptorSet(allocator->cache, allocator->vulkan, layout, debugName);
	assert(allocator->currentSet != VK_NULL_HANDLE);
}

void descriptor_allocator_set_uniform_buffer(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorBufferInfo info)
{
	assert(allocator->currentSet != VK_NULL_HANDLE);

	VkDescriptorBufferInfo* buffer = &allocator->bufferInfos[allocator->bufferCount++];
	VkWriteDescriptorSet* write = &allocator->writes[allocator->writeCount++];

	*buffer = info;
	
	*write = (VkWriteDescriptorSet){
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pBufferInfo = buffer,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.dstSet = allocator->currentSet,
		.dstBinding = binding,
	};
}

void descriptor_allocator_set_storage_buffer(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorBufferInfo info)
{
	assert(allocator->currentSet != VK_NULL_HANDLE);

	VkDescriptorBufferInfo* buffer = &allocator->bufferInfos[allocator->bufferCount++];
	VkWriteDescriptorSet* write = &allocator->writes[allocator->writeCount++];

	*buffer = info;
	
	*write = (VkWriteDescriptorSet){
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pBufferInfo = buffer,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.dstSet = allocator->currentSet,
		.dstBinding = binding,
	};
}

void descriptor_allocator_set_combined_image_sampler(descriptor_allocator_t* allocator, uint32_t binding, VkDescriptorImageInfo info)
{
	assert(allocator->currentSet != VK_NULL_HANDLE);

	VkDescriptorImageInfo* image = &allocator->imageInfos[allocator->imageCount++];
	VkWriteDescriptorSet* write = &allocator->writes[allocator->writeCount++];

	*image = info;
	
	*write = (VkWriteDescriptorSet){
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pImageInfo = image,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.dstSet = allocator->currentSet,
		.dstBinding = binding,
	};
}

VkDescriptorSet descriptor_allocator_end(descriptor_allocator_t* allocator)
{
	vkUpdateDescriptorSets(allocator->vulkan->device, allocator->writeCount, allocator->writes, 0, NULL);

	VkDescriptorSet set = allocator->currentSet;
	allocator->currentSet = VK_NULL_HANDLE;
	return set;
}