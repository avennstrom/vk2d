#include "scene.h"
#include "common.h"
#include "util.h"
#include "vulkan.h"
#include "mat.h"
#include "color.h"

#include <memory.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>

#define SCB_SIZE (16 * 1024)
#define MAX_MODELS (256)

enum scb_command
{
	SCB_CMD_NULL = 0,
	SCB_CMD_DRAW_MODEL,
	SCB_CMD_POINT_LIGHT,
	SCB_CMD_SPOT_LIGHT,
};

typedef struct scb_command_header
{
	enum scb_command command;
	size_t count;
} scb_command_header_t;

typedef struct scb
{
	uint8_t *buf;
	size_t pos;
	size_t size;
	scb_camera_t camera;
} scb_t;

typedef struct model
{
	uint32_t	indexCount;
	uint32_t	indexOffset;
} model_t;

typedef struct scene_frame
{
	VkBuffer				uniformBuffer;
	gpu_frame_uniforms_t*	uniforms;
	VkBuffer				drawBuffer;
	gpu_draw_t*				draws;
	VkBuffer				lightBuffer;
	gpu_light_buffer_t*		lights;
	VkBuffer				spotShadowDrawBuffer;
	VkDrawIndirectCommand*	spotShadowDraws;
} scene_frame_t;

typedef struct scene
{
	vulkan_t*		vulkan;
	scb_t			scb;
	model_t*		models;
	uint64_t*		loadedModelMask;

	VkDeviceMemory	stagingMemory;
	scene_frame_t	frames[FRAME_COUNT];
} scene_t;

struct frame_buffer_desc
{
	size_t offset;
	VkBufferUsageFlags usage;
	VkDeviceSize size;
};

static bool is_model_loaded(const scene_t* scene, model_handle_t handle)
{
	const uint64_t block = (handle.index / 64ull);
	const uint64_t bit = (handle.index % 64ull);
	return (scene->loadedModelMask[block] & (1ull << bit)) != 0;
}

int AllocateSceneStagingMemory(staging_memory_allocator_t* allocator, scene_t *scene)
{
	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		scene_frame_t* frame = &scene->frames[i];

		PushStagingBufferAllocation(allocator, &frame->uniformBuffer, (void**)&frame->uniforms, sizeof(gpu_frame_uniforms_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "Scene Uniforms");
		PushStagingBufferAllocation(allocator, &frame->drawBuffer, (void**)&frame->draws, MAX_DRAWS * sizeof(gpu_draw_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Draws");
		PushStagingBufferAllocation(allocator, &frame->lightBuffer, (void**)&frame->lights, sizeof(gpu_light_buffer_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Lights");
		PushStagingBufferAllocation(allocator, &frame->spotShadowDrawBuffer, (void**)&frame->spotShadowDraws, MAX_SPOT_LIGHTS * sizeof(VkDrawIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, "SpotDraws");
	}

	return 0;
}

scene_t *new_scene(vulkan_t *vulkan)
{
	int r;

	scene_t *scene = calloc(1, sizeof(scene_t));
	if (scene == NULL)
	{
		return NULL;
	}

	scene->vulkan = vulkan;

	scene->scb.buf = malloc(SCB_SIZE);
	scene->scb.size = SCB_SIZE;

	scene->models = calloc(MAX_MODELS, sizeof(model_t));
	scene->loadedModelMask = calloc(MAX_MODELS / 64, sizeof(uint64_t));

	return scene;
}

void delete_scene(scene_t *scene)
{
	vulkan_t* vulkan = scene->vulkan;

	for (size_t i = 0; i < FRAME_COUNT; ++i)
	{
		vkDestroyBuffer(vulkan->device, scene->frames[i].uniformBuffer, NULL);
		vkDestroyBuffer(vulkan->device, scene->frames[i].drawBuffer, NULL);
		vkDestroyBuffer(vulkan->device, scene->frames[i].lightBuffer, NULL);
	}
	vkFreeMemory(vulkan->device, scene->stagingMemory, NULL);

	free(scene->scb.buf);
	free(scene);
}

scb_t *scene_begin(scene_t *scene)
{
	scene->scb.pos = 0;
	return &scene->scb;
}

static void *scb_alloc(scb_t *scb, size_t size)
{
	size_t pos = scb->pos;

	if (pos + size > scb->size)
	{
		return NULL;
	}

	scb->pos += size;
	return scb->buf + pos;
}

static void *scb_append(scb_t *scb, size_t count, enum scb_command command, size_t size)
{
	const size_t allocsize = sizeof(scb_command_header_t) + (size * count);
	void *ptr = scb_alloc(scb, allocsize);
	if (ptr == NULL)
	{
		return NULL;
	}

	scb_command_header_t *header = (scb_command_header_t *)ptr;
	header->command = command;
	header->count = count;

	return (uint8_t *)ptr + sizeof(scb_command_header_t);
}

scb_camera_t *scb_set_camera(scb_t *scb)
{
	return &scb->camera;
}

scb_draw_model_t *scb_draw_models(scb_t *scb, size_t count)
{
	return (scb_draw_model_t *)scb_append(scb, count, SCB_CMD_DRAW_MODEL, sizeof(scb_draw_model_t));
}

scb_point_light_t *scb_add_point_lights(scb_t *scb, size_t count)
{
	return (scb_point_light_t *)scb_append(scb, count, SCB_CMD_POINT_LIGHT, sizeof(scb_point_light_t));
}

scb_spot_light_t *scb_add_spot_lights(scb_t *scb, size_t count)
{
	return (scb_spot_light_t *)scb_append(scb, count, SCB_CMD_SPOT_LIGHT, sizeof(scb_spot_light_t));
}

typedef struct camera
{
	mat4 transform;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
} camera_t;

static void calculate_camera(camera_t *camera, const scb_camera_t *scb_camera, float aspectRatio)
{
	mat4 m = mat_identity();
	m = mat_translate(m, scb_camera->pos);
	m = mat_rotate_y(m, scb_camera->yaw);
	m = mat_rotate_x(m, scb_camera->pitch);

	const mat4 viewMatrix = mat_invert(m);
	const mat4 projectionMatrix = mat_perspective(70.0f, aspectRatio, 0.1f, 256.0f);
	const mat4 viewProjectionMatrix = mat_mul(viewMatrix, projectionMatrix);

	camera->transform = mat_transpose(m);
	camera->viewMatrix = mat_transpose(viewMatrix);
	camera->projectionMatrix = mat_transpose(projectionMatrix);
	camera->viewProjectionMatrix = mat_transpose(viewProjectionMatrix);
}

void DrawScene(
	VkCommandBuffer cb,
	scene_t* scene,
	vulkan_t* vulkan,
	render_targets_t* rt,
	scb_t* scb,
	descriptor_allocator_t* dsalloc,
	staging_memory_context_t* stagingMemory,
	debug_renderer_t* debugRenderer,
	worldgen_t* worldgen,
	painter_t* painter,
	size_t frameIndex)
{
	scb_append(scb, 0, SCB_CMD_NULL, 0);

	uint8_t *ptr = scb->buf;

	scene_frame_t* frame = &scene->frames[frameIndex];

	size_t gpuDrawCount = 0;
	size_t gpuPointLightCount = 0;
	size_t gpuSpotLightCount = 0;

	for (;;)
	{
		scb_command_header_t* header = (scb_command_header_t*)ptr;
		if (header->command == SCB_CMD_NULL)
		{
			break;
		}

		void *data = ptr + sizeof(scb_command_header_t);
		size_t cmdsize = 0;

		gpu_point_light_t* gpuPointLights = frame->lights->pointLights;
		gpu_spot_light_t* gpuSpotLights = frame->lights->spotLights;
		mat4* spotLightMatrices = frame->lights->spotLightMatrices;

		switch (header->command)
		{
		case SCB_CMD_DRAW_MODEL:
			cmdsize = sizeof(scb_draw_model_t);
			scb_draw_model_t *draws = (scb_draw_model_t *)data;
			for (size_t i = 0; i < header->count; ++i)
			{
				if (is_model_loaded(scene, draws[i].model)) {
					frame->draws[gpuDrawCount++] = (gpu_draw_t){
						.transform = draws[i].transform,
						.modelIndex = draws[i].model.index,
					};
				}
			}
			break;
		case SCB_CMD_POINT_LIGHT:
			cmdsize = sizeof(scb_point_light_t);
			scb_point_light_t *pointLights = (scb_point_light_t *)data;
			for (size_t i = 0; i < header->count; ++i)
			{
				gpuPointLights[gpuPointLightCount++] = (gpu_point_light_t){
					.pos = pointLights[i].position,
					.radius = pointLights[i].radius,
					.color = pointLights[i].color,
				};
			}
			break;
		case SCB_CMD_SPOT_LIGHT:
			cmdsize = sizeof(scb_spot_light_t);
			scb_spot_light_t *spotLights = (scb_spot_light_t *)data;
			for (size_t i = 0; i < header->count; ++i)
			{
				scb_spot_light_t *spotLight = spotLights + i;

				//mat4 matrix = mat_identity();
				//matrix = mat_translate(matrix, spotLight->position);

				mat4 viewMatrix = mat_invert(spotLight->transform);
				mat4 projectionMatrix = mat_perspective(spotLight->radius, 1.0f, 0.1f, spotLight->range);
				mat4 viewProjectionMatrix = mat_mul(viewMatrix, projectionMatrix);

				vec3 pos = { spotLight->transform.r3.x, spotLight->transform.r3.y, spotLight->transform.r3.z };

				spotLightMatrices[gpuSpotLightCount] = mat_transpose(viewProjectionMatrix);

				gpuSpotLights[gpuSpotLightCount] = (gpu_spot_light_t){
					.pos = pos,
					.range = spotLight->range,
					.color = spotLight->color,
				};

				++gpuSpotLightCount;
			}
			break;
		}

		ptr += sizeof(scb_command_header_t) + header->count * cmdsize;
	}

	float aspectRatio = rt->resolution.width / (float)rt->resolution.height;

	camera_t camera;
	calculate_camera(&camera, &scb->camera, aspectRatio);

	worldgen_info_t worldgenInfo;
	GetWorldgenInfo(&worldgenInfo, worldgen);

	//printf("spot lights: %d\n", gpuSpotLightCount);

	(*frame->uniforms) = (gpu_frame_uniforms_t){
		.matViewProj		= camera.viewProjectionMatrix,
		.pointLightCount	= gpuPointLightCount,
		.spotLightCount		= gpuSpotLightCount,
		.drawCount			= gpuDrawCount,
	};

	PushStagingMemoryFlush(stagingMemory, frame->uniforms, sizeof(gpu_frame_uniforms_t));
	PushStagingMemoryFlush(stagingMemory, frame->lights, sizeof(gpu_light_buffer_t));
	PushStagingMemoryFlush(stagingMemory, frame->spotShadowDraws, MAX_SPOT_LIGHTS * sizeof(VkDrawIndirectCommand));

	const VkDescriptorBufferInfo frameUniformBuffer = {
		.buffer = frame->uniformBuffer,
		.range = sizeof(gpu_frame_uniforms_t),
	};
	const VkDescriptorBufferInfo lightBuffer = {
		.buffer = frame->lightBuffer,
		.range = VK_WHOLE_SIZE,
	};
	const VkDescriptorImageInfo spotLightAtlas = {
		.sampler = vulkan->pointClampSampler,
		.imageView = rt->spotLightAtlas.view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkDescriptorSet worldDescriptorSet = CreateWorldDescriptorSet(
		worldgen,
		dsalloc,
		frameUniformBuffer,
		lightBuffer,
		GetPaintingImage(painter),
		spotLightAtlas);

	{
		const VkImageMemoryBarrier imageBarriers[] = {
			{
				// Spot Light Atlas -> Attachment Write
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.image = rt->spotLightAtlas.image,
				.srcAccessMask = VK_ACCESS_NONE,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.layerCount = MAX_SPOT_LIGHTS,
					.levelCount = 1,
				},
			},
		};
		vkCmdPipelineBarrier(cb,
							 VK_PIPELINE_STAGE_NONE,
							 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
							 0,
							 0, NULL,
							 0, NULL,
							 countof(imageBarriers), imageBarriers);
	}
	{
		const VkRenderingAttachmentInfo depthAttachment = {
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = rt->spotLightAtlas.view,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue.depthStencil.depth = 1.0f,
		};
		const VkRenderingInfo renderingInfo = {
			VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pDepthAttachment = &depthAttachment,
			.layerCount = gpuSpotLightCount,
			.renderArea = {
				.extent = {SPOT_LIGHT_RESOLUTION, SPOT_LIGHT_RESOLUTION},
			},
		};
		vkCmdBeginRendering(cb, &renderingInfo);

		for (size_t i = 0; i < gpuSpotLightCount; ++i) {
			frame->spotShadowDraws[i].vertexCount	= worldgenInfo.vertexCount;
			frame->spotShadowDraws[i].instanceCount	= 1;
			frame->spotShadowDraws[i].firstVertex	= 0;
			frame->spotShadowDraws[i].firstInstance	= i;
		}

		SetViewportAndScissor(cb, (VkOffset2D){0, 0}, (VkExtent2D){SPOT_LIGHT_RESOLUTION, SPOT_LIGHT_RESOLUTION});
		
		const VkDeviceSize vertexBufferOffset = 0;
		vkCmdBindVertexBuffers(cb, 0, 1, &worldgenInfo.vertexBuffer, &vertexBufferOffset);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, worldgenInfo.pipelineLayout, 0, 1, &worldDescriptorSet, 0, NULL);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, worldgenInfo.shadowPipeline);
		vkCmdDrawIndirect(cb, frame->spotShadowDrawBuffer, 0, gpuSpotLightCount, sizeof(VkDrawIndirectCommand));

		vkCmdEndRendering(cb);
	}
	{
		const VkImageMemoryBarrier imageBarriers[] = {
			{
				// Spot Light Atlas -> Shader Read
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.image = rt->spotLightAtlas.image,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.layerCount = MAX_SPOT_LIGHTS,
					.levelCount = 1,
				},
			},
			{
				// Scene Color -> Attachment Write
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.image = rt->sceneColor.image,
				.srcAccessMask = VK_ACCESS_NONE,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.layerCount = 1,
					.levelCount = 1,
				},
			},
		};
		vkCmdPipelineBarrier(cb,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, NULL,
			0, NULL,
			countof(imageBarriers), imageBarriers);
	}
	{
		const VkRenderingAttachmentInfo colorAttachments[] = {
			{
				VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = rt->sceneColor.view,
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				// .resolveImageView = backbufferView,
				// .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				// .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			}};
		const VkRenderingAttachmentInfo depthAttachment = {
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = rt->sceneDepth.view,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue.depthStencil.depth = 1.0f,
		};

		const VkRenderingInfo renderingInfo = {
			VK_STRUCTURE_TYPE_RENDERING_INFO,
			.colorAttachmentCount = countof(colorAttachments),
			.pColorAttachments = colorAttachments,
			.pDepthAttachment = &depthAttachment,
			.layerCount = 1,
			.renderArea = {
				.extent = rt->resolution,
			}};
		vkCmdBeginRendering(cb, &renderingInfo);
		{
			SetViewportAndScissor(cb, (VkOffset2D){}, rt->resolution);

			DrawWorld(
				cb,
				worldgen,
				worldDescriptorSet);
			
			FlushDebugRenderer(
				cb, 
				stagingMemory, 
				debugRenderer,
				dsalloc,
				frameUniformBuffer,
				frameIndex);
		}
		vkCmdEndRendering(cb);
	}
}