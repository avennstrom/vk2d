#pragma once

#include <vulkan/vulkan_core.h>

typedef struct vulkan_staging_memory_range {
	void* ptr;
	size_t size;
} vulkan_staging_memory_range_t;

typedef struct vulkan
{
	VkInstance					instance;
	VkPhysicalDevice			physicalDevice;
	VkPhysicalDeviceLimits		physicalDeviceLimits;
	VkDevice					device;
	VkQueue						mainQueue;
	uint32_t					mainQueueFamilyIndex;
	VkSurfaceFormatKHR			surfaceFormat;
	VkCommandPool				commandPool;

	VkSampler					pointClampSampler;
	VkSampler					linearClampSampler;

	size_t							flushRangeCount;
	vulkan_staging_memory_range_t	flushRanges[256];

	PFN_vkSetDebugUtilsObjectNameEXT	vkSetDebugUtilsObjectNameEXT;
} vulkan_t;

int CreateVulkanContext(vulkan_t* vulkan, VkInstance instance, VkSurfaceKHR surface);
const char* VkResultString(VkResult result);
uint32_t FindMemoryType(vulkan_t* vulkan, uint32_t typeFilter, VkMemoryPropertyFlags properties);
VkBuffer CreateBuffer(VkDeviceMemory* memory, vulkan_t* vulkan, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties);
VkImage CreateImage(VkDeviceMemory* memory, vulkan_t* vulkan, const VkImageCreateInfo* imageCreateInfo, VkMemoryPropertyFlags memoryProperties);
VkImage CreateImage2D(VkDeviceMemory* memory, vulkan_t* vulkan, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties);
void SetViewportAndScissor(VkCommandBuffer cb, VkOffset2D offset, VkExtent2D size);

void SetShaderModuleName(vulkan_t* vulkan, VkShaderModule module, const char* name);
void SetBufferName(vulkan_t* vulkan, VkBuffer buffer, const char* name);
void SetBufferViewName(vulkan_t* vulkan, VkBufferView bufferView, const char* name);
void SetImageName(vulkan_t* vulkan, VkImage image, const char* name);
void SetImageViewName(vulkan_t* vulkan, VkImageView imageView, const char* name);
void SetDeviceMemoryName(vulkan_t* vulkan, VkDeviceMemory memory, const char* name);
void SetDescriptorSetName(vulkan_t* vulkan, VkDescriptorSet descriptorSet, const char* name);
void SetDescriptorSetLayoutName(vulkan_t* vulkan, VkDescriptorSetLayout descriptorSetLayout, const char* name);
void SetPipelineLayoutName(vulkan_t* vulkan, VkPipelineLayout pipelineLayout, const char* name);