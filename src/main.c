#include "common.h"
#include "util.h"
#include "window.h"
#include "vulkan.h"
#include "shaders.h"
#include "color.h"
#include "debug_renderer.h"
#include "worldgen.h"
#include "descriptors.h"
#include "mat.h"
#include "game.h"
#include "scene.h"
#include "render_targets.h"
#include "staging_memory.h"
#include "composite.h"
#include "delta_time.h"
#include "painter.h"

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <float.h>
#include <string.h>

#include <vulkan/vulkan.h>

#define FRAME_UNIFORM_SIZE (64 * 1024)

enum
{
	FRAME_IN_FLIGHT = (1u << 0u),
};

typedef struct frame
{
	uint32_t flags;
	VkCommandBuffer cb;
	VkFence fence;
	VkSemaphore backbufferAvailable;
	VkSemaphore backbufferDone;
} frame_t;

// typedef struct image_upload
// {
// 	VkBuffer src;
// 	size_t srcOffset;
// 	size_t srcSize;
// 	VkImage dst;
// } image_upload_t;

typedef struct image_upload_barrier
{
	VkImage image;
	VkImageLayout oldLayout;
	VkImageLayout newLayout;
} image_upload_barrier_t;

typedef struct swapchain {
	VkSwapchainKHR swapchain;
	VkImage backbuffer[8];
	VkImageView backbufferView[8];
	size_t backbufferCount;
} swapchain_t;

typedef struct app
{
	frame_t frames[FRAME_COUNT];
	size_t currentFrame;
	VkDeviceMemory frameUniformBufferMemory;
	VkBuffer frameUniformBuffer;
	void *frameUniformData;

	//image_upload_t uploads[32];
	// size_t uploadCount;
	// image_upload_barrier_t uploadBarriers[32];
	// size_t uploadBarrierCount;
} app_t;

static int CreateSwapchain(swapchain_t* swapchain, vulkan_t *vulkan, VkSurfaceKHR surface, VkExtent2D resolution);
static void DestroySwapchain(swapchain_t* swapchain, vulkan_t *vulkan);

// static void QueueImageUpload(app_t *app, const image_upload_t *upload)
// {
// 	assert(app->uploadCount < countof(app->uploads));
// 	app->uploads[app->uploadCount++] = *upload;
// }

// static void QueueImageUploadTransition(app_t *app, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
// {
// 	assert(app->uploadBarrierCount < countof(app->uploadBarriers));
// 	app->uploadBarriers[app->uploadBarrierCount++] = (image_upload_barrier_t){
// 		.image = image,
// 		.oldLayout = oldLayout,
// 		.newLayout = newLayout,
// 	};
// }

int InitFrame(frame_t *frame, vulkan_t *vulkan)
{
	VkResult r;

	const VkCommandBufferAllocateInfo allocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = vulkan->commandPool,
		.commandBufferCount = 1,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	};
	r = vkAllocateCommandBuffers(vulkan->device, &allocateInfo, &frame->cb);
	if (r != VK_SUCCESS)
	{
		fprintf(stderr, "vkAllocateCommandBuffers failed\n");
		return 1;
	}

	const VkFenceCreateInfo fenceInfo = {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};
	r = vkCreateFence(vulkan->device, &fenceInfo, NULL, &frame->fence);
	if (r != VK_SUCCESS)
	{
		fprintf(stderr, "vkCreateFence failed\n");
		return 1;
	}

	const VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	r = vkCreateSemaphore(vulkan->device, &semaphoreInfo, NULL, &frame->backbufferAvailable);
	if (r != VK_SUCCESS)
	{
		fprintf(stderr, "vkCreateSemaphore failed\n");
		return 1;
	}

	r = vkCreateSemaphore(vulkan->device, &semaphoreInfo, NULL, &frame->backbufferDone);
	if (r != VK_SUCCESS)
	{
		fprintf(stderr, "vkCreateSemaphore failed\n");
		return 1;
	}

	return 0;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugMessageCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData)
{
	fprintf(stderr, "%s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

int main(int argc, char **argv)
{
	srand(time(NULL));

	app_t app = {};

	int r;
	VkResult vkr;

	const VkApplicationInfo appInfo = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.apiVersion = VK_API_VERSION_1_3,
		.pEngineName = "forsengine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.pApplicationName = "frsn",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
	};

	const char *instanceExtensions[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,
		getWindowSurfaceExtensionName(),
	};

	const VkInstanceCreateInfo instanceInfo = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.ppEnabledExtensionNames = instanceExtensions,
		.enabledExtensionCount = countof(instanceExtensions),
	};

	VkInstance instance;
	vkr = vkCreateInstance(&instanceInfo, NULL, &instance);
	if (vkr != VK_SUCCESS)
	{
		fprintf(stderr, "vkCreateInstance failed\n", VkResultString(vkr));
		return 1;
	}
	assert(instance != VK_NULL_HANDLE);

	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

	if (vkCreateDebugUtilsMessengerEXT != NULL)
	{
		const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
			VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = VulkanDebugMessageCallback,
			.pUserData = NULL,
		};
		vkr = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCreateInfo, NULL, &debugMessenger);
		if (vkr != VK_SUCCESS)
		{
			fprintf(stderr, "Failed to create vulkan debug messenger\n");
		}
	}

	VkExtent2D resolution = {1280, 720};

	window_t* window = createWindow(resolution.width, resolution.height);
	if (window == NULL)
	{
		fprintf(stderr, "Failed to create OS window\n");
		return 1;
	}

	VkSurfaceKHR surface = createWindowSurface(instance, window);
	if (surface == VK_NULL_HANDLE)
	{
		fprintf(stderr, "Failed to create window surface\n");
		return 1;
	}

	vulkan_t vulkan = {};
	r = CreateVulkanContext(&vulkan, instance, surface);
	if (r != 0)
	{
		fprintf(stderr, "Failed to create vulkan context\n");
		return 1;
	}

	swapchain_t swapchain = {};
	r = CreateSwapchain(&swapchain, &vulkan, surface, resolution);
	assert(r == 0);

	render_targets_t rt = {};
	r = CreateRenderTargets(&rt, &vulkan, resolution);
	assert(r == 0);

	r = InitShaderLibrary(&vulkan);
	assert(r == 0);

	const debug_renderer_config_t debugRendererConfig = {
		.maxPoints = 1024 * 1024,
		.maxLines = 512 * 1024,
		.maxTriangles = 128 * 1024,
	};
	debug_renderer_t debugRenderer = {};
	r = CreateDebugRenderer(&debugRenderer, &vulkan, &debugRendererConfig);
	assert(r == 0);

	{
		for (uint32_t i = 0; i < countof(app.frames); ++i)
		{
			frame_t *frame = &app.frames[i];
			if (InitFrame(frame, &vulkan) != 0)
			{
				return 1;
			}
		}
	}

	const uint32_t maxDescriptorSets = 1024;

	const VkDescriptorPoolSize descriptorPoolSizes[] = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxDescriptorSets},
	};

	const VkDescriptorPoolCreateInfo descriptorPoolInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = maxDescriptorSets,
		.poolSizeCount = countof(descriptorPoolSizes),
		.pPoolSizes = descriptorPoolSizes,
	};

	VkDescriptorPool descriptorPool;
	vkr = vkCreateDescriptorPool(vulkan.device, &descriptorPoolInfo, NULL, &descriptorPool);
	if (vkr != VK_SUCCESS)
	{
		return 1;
	}

	descriptor_set_cache_t dscache;
	CreateDescriptorSetCache(&dscache, descriptorPool, maxDescriptorSets);

	descriptor_allocator_t dsalloc;
	CreateDescriptorAllocator(&dsalloc, &dscache, &vulkan, 64);

	uint64_t frameId = 0;

	worldgen_t* worldgen = CreateWorldgen(&vulkan);
	painter_t* painter = CreatePainter(&vulkan);
	game_t *game = game_create(window, worldgen);
	scene_t *scene = new_scene(&vulkan);
	composite_t* composite = create_composite(&vulkan);

	staging_memory_allocator_t staging_allocator;
	ResetStagingMemoryAllocator(&staging_allocator, &vulkan);
	AllocateSceneStagingMemory(&staging_allocator, scene);
	AllocateDebugRendererStagingMemory(&staging_allocator, &debugRenderer);
	AllocateWorldgenStagingMemory(&staging_allocator, worldgen);
	AllocatePainterStagingMemory(&staging_allocator, painter);

	staging_memory_allocation_t stagingAllocation;
	FinalizeStagingMemoryAllocator(&stagingAllocation, &staging_allocator);

	delta_timer_t deltaTimer;
	ResetDeltaTime(&deltaTimer);

	for (;;)
	{
		//
		// ---- poll events ----
		//
		window_event_t event;
		while (pollWindowEvent(&event, window))
		{
			game_window_event(game, &event);
		}

		//
		// --- maybe render ---
		//
		frame_t *frame = &app.frames[app.currentFrame];

		if (frame->flags & FRAME_IN_FLIGHT)
		{
			vkr = vkWaitForFences(vulkan.device, 1, &frame->fence, VK_TRUE, 0);
			if (vkr == VK_TIMEOUT)
			{
				continue;
			}
			assert(vkr == VK_SUCCESS);

			vkr = vkResetFences(vulkan.device, 1, &frame->fence);
			assert(vkr == VK_SUCCESS);

			frame->flags &= ~FRAME_IN_FLIGHT;
		}

		uint32_t imageIndex;
		vkr = vkAcquireNextImageKHR(vulkan.device, swapchain.swapchain, 0, frame->backbufferAvailable, VK_NULL_HANDLE, &imageIndex);
		if (vkr == VK_TIMEOUT)
		{
			printf("vkAcquireNextImageKHR timeout\n");
			continue;
		}
		else if (vkr != VK_SUCCESS)
		{
			fprintf(stderr, "vkAcquireNextImageKHR failed: %s\n", VkResultString(vkr));
			return 1;
		}

		//
		// --- game logic ---
		//
		const float deltaTime = (float)CaptureDeltaTime(&deltaTimer);

		MakeCurrentDebugRenderer(&debugRenderer);
		
		TickPainter(painter, frameId);
		TickWorldgen(worldgen);
		TickGame(game, deltaTime);

		//
		// ---- render ----
		//
		UpdateDescriptorSetCache(&dscache, frameId);

		staging_memory_context_t stagingMemoryContext;
		ResetStagingMemoryContext(&stagingMemoryContext, &stagingAllocation);

		scb_t* scb = scene_begin(scene);
		assert(scb != NULL);

		game_render(scb, game);

		VkCommandBuffer cb = frame->cb;

		const VkCommandBufferBeginInfo cbBeginInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};
		vkr = vkBeginCommandBuffer(cb, &cbBeginInfo);
		assert(vkr == VK_SUCCESS);

		DrawDebugLine((debug_vertex_t){0.0f, 0.0f, 0.0f, COLOR_RED}, (debug_vertex_t){5.0f, 0.0f, 0.0f, COLOR_RED});
		DrawDebugLine((debug_vertex_t){0.0f, 0.0f, 0.0f, COLOR_GREEN}, (debug_vertex_t){0.0f, 5.0f, 0.0f, COLOR_GREEN});
		DrawDebugLine((debug_vertex_t){0.0f, 0.0f, 0.0f, COLOR_BLUE}, (debug_vertex_t){0.0f, 0.0f, 5.0f, COLOR_BLUE});

		RenderPaintings(
			cb,
			painter,
			&stagingMemoryContext,
			frameId);

		DrawScene(
			cb, 
			scene,
			&vulkan,
			&rt, 
			scb, 
			&dsalloc,
			&stagingMemoryContext,
			&debugRenderer,
			worldgen,
			painter,
			app.currentFrame);

		const VkImage backbufferImage = swapchain.backbuffer[imageIndex];
		const VkImageView backbufferView = swapchain.backbufferView[imageIndex];

		{
			const VkImageMemoryBarrier imageBarriers[] = {
				{
					// Scene Color -> Read
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.image = rt.sceneColor.image,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.layerCount = 1,
						.levelCount = 1,
					},
				},
				{
					// Backbuffer -> Color Attachment
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.image = backbufferImage,
					.srcAccessMask = VK_ACCESS_NONE,
					.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.layerCount = 1,
						.levelCount = 1,
					},
				}};
			vkCmdPipelineBarrier(cb,
								 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
								 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								 0,
								 0, NULL,
								 0, NULL,
								 countof(imageBarriers), imageBarriers);
		}
		
		draw_composite(
			cb,
			&vulkan,
			composite,
			&dsalloc,
			&rt, 
			backbufferView);

		{
			const VkImageMemoryBarrier imageBarriers[] = {
				{
					// Backbuffer -> Present
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.image = backbufferImage,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.layerCount = 1,
						.levelCount = 1,
					},
				}};
			vkCmdPipelineBarrier(cb,
								 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
								 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 0,
								 0, NULL,
								 0, NULL,
								 countof(imageBarriers), imageBarriers);
		}

		vkr = vkEndCommandBuffer(cb);
		assert(vkr == VK_SUCCESS);

		MakeCurrentDebugRenderer(NULL);

		vkr = FlushStagingMemory(&stagingMemoryContext, &vulkan);
		assert(vkr == VK_SUCCESS);

		const VkPipelineStageFlags waitDstStageFlags = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		const VkSubmitInfo submitInfo = {
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &cb,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &frame->backbufferAvailable,
			.pWaitDstStageMask = &waitDstStageFlags,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &frame->backbufferDone,
		};
		vkr = vkQueueSubmit(vulkan.mainQueue, 1, &submitInfo, frame->fence);
		if (vkr != VK_SUCCESS)
		{
			fprintf(stderr, "vkQueueSubmit failed: %s\n", VkResultString(vkr));
			continue;
		}

		const VkPresentInfoKHR presentInfo = {
			VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.swapchainCount = 1,
			.pSwapchains = &swapchain.swapchain,
			.pImageIndices = &imageIndex,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &frame->backbufferDone,
		};
		vkr = vkQueuePresentKHR(vulkan.mainQueue, &presentInfo);
		if (vkr == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// query new resolution
			VkSurfaceCapabilitiesKHR surfaceCapabilities;
			vkr = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan.physicalDevice, surface, &surfaceCapabilities);
			assert(vkr == VK_SUCCESS);
			assert(surfaceCapabilities.minImageExtent.width == surfaceCapabilities.maxImageExtent.width);
			assert(surfaceCapabilities.minImageExtent.height == surfaceCapabilities.maxImageExtent.height);
			resolution = surfaceCapabilities.maxImageExtent;

			vkQueueWaitIdle(vulkan.mainQueue);
			DestroySwapchain(&swapchain, &vulkan);
			DestroyRenderTargets(&rt, &vulkan);
			CreateRenderTargets(&rt, &vulkan, resolution);
			CreateSwapchain(&swapchain, &vulkan, surface, resolution);
		}
		else
		{
			if (vkr != VK_SUCCESS)
			{
				fprintf(stderr, "vkQueuePresentKHR failed\n");
				return 1;
			}
		}

		frame->flags |= FRAME_IN_FLIGHT;
		app.currentFrame = (app.currentFrame + 1) % FRAME_COUNT;

		++frameId;
	}

	vkDeviceWaitIdle(vulkan.device);

	destroyWindow(window);
	vkDestroySurfaceKHR(instance, surface, NULL);
	vkDestroyDevice(vulkan.device, NULL);
	if (vkDestroyDebugUtilsMessengerEXT != NULL)
	{
		vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
	}
	vkDestroyInstance(instance, NULL);
	return 0;
}

static int CreateSwapchain(swapchain_t* swapchain, vulkan_t *vulkan, VkSurfaceKHR surface, VkExtent2D resolution)
{
	VkResult r;

	assert(vulkan->surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

	const VkSwapchainCreateInfoKHR swapchainInfo = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.imageFormat = vulkan->surfaceFormat.format,
		.imageColorSpace = vulkan->surfaceFormat.colorSpace,
		.imageExtent = resolution,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageArrayLayers = 1,
		.minImageCount = 2,
		.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &vulkan->mainQueueFamilyIndex,
	};
	r = vkCreateSwapchainKHR(vulkan->device, &swapchainInfo, NULL, &swapchain->swapchain);
	if (r != VK_SUCCESS)
	{
		fprintf(stderr, "vkCreateSwapchainKHR failed\n", r);
		return 1;
	}

	uint32_t backbufferCount;
	r = vkGetSwapchainImagesKHR(vulkan->device, swapchain->swapchain, &backbufferCount, NULL);
	assert(r == VK_SUCCESS);
	assert(backbufferCount <= countof(swapchain->backbuffer));
	r = vkGetSwapchainImagesKHR(vulkan->device, swapchain->swapchain, &backbufferCount, swapchain->backbuffer);
	assert(r == VK_SUCCESS);

	swapchain->backbufferCount = backbufferCount;

	for (uint32_t i = 0; i < backbufferCount; ++i)
	{
		const VkImageViewCreateInfo backbufferViewCreateInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain->backbuffer[i],
			.format = vulkan->surfaceFormat.format,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1,
			},
		};
		r = vkCreateImageView(vulkan->device, &backbufferViewCreateInfo, NULL, &swapchain->backbufferView[i]);
		assert(r == VK_SUCCESS);

		SetImageName(vulkan, swapchain->backbuffer[i], "Backbuffer");
		SetImageViewName(vulkan, swapchain->backbufferView[i], "Backbuffer");
	}

	return 0;
}

static void DestroySwapchain(swapchain_t* swapchain, vulkan_t *vulkan)
{
	for (size_t i = 0; i < swapchain->backbufferCount; ++i)
	{
		vkDestroyImageView(vulkan->device, swapchain->backbufferView[i], NULL);
		swapchain->backbufferView[i] = VK_NULL_HANDLE;
		swapchain->backbuffer[i] = VK_NULL_HANDLE;
	}

	vkDestroySwapchainKHR(vulkan->device, swapchain->swapchain, NULL);
	swapchain->swapchain = VK_NULL_HANDLE;
}