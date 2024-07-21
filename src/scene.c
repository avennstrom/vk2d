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
#define MAX_INSTANCES (16 * 1024)
#define MESH_DATA_BUFFER_SIZE (256 * 1024 * 1024) // triangles and indices

typedef struct model model_t;

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
	uint8_t*		buf;
	size_t			pos;
	size_t			size;
	scb_camera_t	camera;
} scb_t;

typedef struct scene_frame
{
	VkBuffer						uniformBuffer;
	gpu_frame_uniforms_t*			uniforms;
	VkBuffer						drawBuffer;
	gpu_draw_t*						draws;
	VkBuffer						lightBuffer;
	gpu_light_buffer_t*				lights;
	VkBuffer						spotShadowDrawBuffer;
	VkDrawIndirectCommand*			spotShadowDraws;
} scene_frame_t;

typedef struct scene
{
	vulkan_t*		vulkan;
	scb_t			scb;
	model_t*		models;
	uint64_t*		loadedModelMask;
	model_t*		loadingModels;

	VkBuffer		vertexBuffer;
	VkDeviceMemory	vertexBufferMemory;

	VkDeviceMemory	stagingMemory;

	VkDescriptorSetLayout	modelDescriptorSetLayout;
	VkPipelineLayout		modelPipelineLayout;
	VkPipeline				modelPipeline;

	scene_frame_t	frames[FRAME_COUNT];
} scene_t;

static int scene_create_pipelines(scene_t* scene, vulkan_t* vulkan)
{
	const VkDescriptorSetLayoutBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
	};
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = countof(bindings),
		.pBindings = bindings,
	};
	if (vkCreateDescriptorSetLayout(vulkan->device, &descriptorSetLayoutInfo, NULL, &scene->modelDescriptorSetLayout) != VK_SUCCESS) {
		return 1;
	}
	SetDescriptorSetLayoutName(vulkan, scene->modelDescriptorSetLayout, "Scene");

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &scene->modelDescriptorSetLayout,
	};
	if (vkCreatePipelineLayout(vulkan->device, &pipelineLayoutInfo, NULL, &scene->modelPipelineLayout) != VK_SUCCESS) {
		return 1;
	}
	SetPipelineLayoutName(vulkan, scene->modelPipelineLayout, "Scene");
	
	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = g_shaders.modules[SHADER_MODEL_VERT],
			.pName = "vs_main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = g_shaders.modules[SHADER_MODEL_FRAG],
			.pName = "fs_main",
		},
	};

	// const VkVertexInputBindingDescription vertexBindings[] = {
	// 	{
	// 		.binding = 0,
	// 		.stride = sizeof(debug_vertex_t),
	// 		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	// 	}
	// };

	// const VkVertexInputAttributeDescription vertexAttributes[] = {
	// 	{
	// 		.binding = 0,
	// 		.location = 0,
	// 		.format = VK_FORMAT_R32G32B32_SFLOAT,
	// 	},
	// 	{
	// 		.binding = 0,
	// 		.location = 1,
	// 		.format = VK_FORMAT_R8G8B8A8_UNORM,
	// 		.offset = offsetof(debug_vertex_t, color),
	// 	}
	// };

	const VkPipelineVertexInputStateCreateInfo vertexInput = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		// .vertexBindingDescriptionCount = countof(vertexBindings),
		// .pVertexBindingDescriptions = vertexBindings,
		// .vertexAttributeDescriptionCount = countof(vertexAttributes),
		// .pVertexAttributeDescriptions = vertexAttributes,
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssembler = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	const VkPipelineRasterizationStateCreateInfo rasterizer = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.cullMode = VK_CULL_MODE_NONE,
		.lineWidth = 1.0f,
	};

	const VkPipelineMultisampleStateCreateInfo multisampling = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	const VkPipelineColorBlendAttachmentState blendAttachments[] = {
		{
			.blendEnable = VK_TRUE,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
		},
	};

	const VkPipelineColorBlendStateCreateInfo colorBlending = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = blendAttachments,
	};

	const VkPipelineViewportStateCreateInfo viewportState = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	const VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	
	const VkPipelineDynamicStateCreateInfo dynamicState = {
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = countof(dynamicStates),
		.pDynamicStates = dynamicStates,
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
	};

	const VkFormat colorFormats[] = { SCENE_COLOR_FORMAT };
	const VkPipelineRenderingCreateInfo renderingInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = countof(colorFormats),
		.pColorAttachmentFormats = colorFormats,
		.depthAttachmentFormat = SCENE_DEPTH_FORMAT,
	};

	const VkGraphicsPipelineCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderingInfo,
		.stageCount = countof(stages),
		.pStages = stages,
		.layout = scene->modelPipelineLayout,
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembler,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pColorBlendState = &colorBlending,
		.pViewportState = &viewportState,
		.pDynamicState = &dynamicState,
		.pDepthStencilState = &depthStencilState,
	};

	if (vkCreateGraphicsPipelines(vulkan->device, NULL, 1, &createInfo, NULL, &scene->modelPipeline) != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines failed\n");
		return 1;
	}

	return 0;
}

int scene_alloc_staging_mem(staging_memory_allocator_t* allocator, scene_t *scene)
{
	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		scene_frame_t* frame = &scene->frames[i];

		PushStagingBufferAllocation(allocator, &frame->uniformBuffer, (void**)&frame->uniforms, sizeof(gpu_frame_uniforms_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "Scene Uniforms");
		PushStagingBufferAllocation(allocator, &frame->drawBuffer, (void**)&frame->draws, MAX_DRAWS * sizeof(gpu_draw_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, "Draws");
		//PushStagingBufferAllocation(allocator, &frame->drawCommandBuffer, (void**)&frame->drawCommands, MAX_DRAWS * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Draw Commands");
		PushStagingBufferAllocation(allocator, &frame->lightBuffer, (void**)&frame->lights, sizeof(gpu_light_buffer_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Lights");
		PushStagingBufferAllocation(allocator, &frame->spotShadowDrawBuffer, (void**)&frame->spotShadowDraws, MAX_SPOT_LIGHTS * sizeof(VkDrawIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, "SpotDraws");
	}

	return 0;
}

scene_t *scene_create(vulkan_t *vulkan)
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

	r = scene_create_pipelines(scene, vulkan);
	assert(r == 0);

	//scene->vertexBuffer = CreateBuffer(&scene->vertexBufferMemory, vulkan, 1024 * 1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	return scene;
}

void scene_destroy(scene_t *scene)
{
	vulkan_t* vulkan = scene->vulkan;

	for (size_t i = 0; i < FRAME_COUNT; ++i)
	{
		vkDestroyBuffer(vulkan->device, scene->frames[i].uniformBuffer, NULL);
		vkDestroyBuffer(vulkan->device, scene->frames[i].drawBuffer, NULL);
		vkDestroyBuffer(vulkan->device, scene->frames[i].lightBuffer, NULL);
		vkDestroyBuffer(vulkan->device, scene->frames[i].spotShadowDrawBuffer, NULL);
	}
	vkFreeMemory(vulkan->device, scene->stagingMemory, NULL);

	vkDestroyPipeline(vulkan->device, scene->modelPipeline, NULL);
	vkDestroyPipelineLayout(vulkan->device, scene->modelPipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(vulkan->device, scene->modelDescriptorSetLayout, NULL);

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

void scene_draw(
	VkCommandBuffer cb,
	scene_t* scene,
	scb_t* scb,
	const render_context_t* rc,
	debug_renderer_t* debugRenderer)
{
	scb_append(scb, 0, SCB_CMD_NULL, 0);

	uint8_t *ptr = scb->buf;

	scene_frame_t* frame = &scene->frames[rc->frameIndex];

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
		gpu_draw_t* gpuDraws = frame->draws;
		mat4* spotLightMatrices = frame->lights->spotLightMatrices;

		switch (header->command)
		{
		case SCB_CMD_DRAW_MODEL:
			cmdsize = sizeof(scb_draw_model_t);
			scb_draw_model_t *draws = (scb_draw_model_t *)data;
			for (size_t i = 0; i < header->count; ++i)
			{
				model_info_t modelInfo;
				if (model_loader_get_model_info(&modelInfo, rc->modelLoader, draws[i].model))
				{
					gpuDraws[gpuDrawCount++] = (gpu_draw_t){
						.indexCount = modelInfo.indexCount,
						.instanceCount = 1,
						.firstIndex = modelInfo.indexOffset,
						.vertexPositionOffset = modelInfo.vertexPositionOffset,
						.vertexNormalOffset = modelInfo.vertexNormalOffset,
						.transform = draws[i].transform,
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

	float aspectRatio = rc->rt->resolution.width / (float)rc->rt->resolution.height;

	camera_t camera;
	calculate_camera(&camera, &scb->camera, aspectRatio);

	(*frame->uniforms) = (gpu_frame_uniforms_t){
		.matViewProj		= camera.viewProjectionMatrix,
		.pointLightCount	= gpuPointLightCount,
		.spotLightCount		= gpuSpotLightCount,
		.drawCount			= gpuDrawCount,
	};

	PushStagingMemoryFlush(rc->stagingMemory, frame->uniforms, sizeof(gpu_frame_uniforms_t));
	PushStagingMemoryFlush(rc->stagingMemory, frame->lights, sizeof(gpu_light_buffer_t));
	PushStagingMemoryFlush(rc->stagingMemory, frame->spotShadowDraws, MAX_SPOT_LIGHTS * sizeof(VkDrawIndirectCommand));
	PushStagingMemoryFlush(rc->stagingMemory, frame->draws, gpuDrawCount * sizeof(gpu_draw_t));

	const VkDescriptorBufferInfo frameUniformBuffer = {
		.buffer = frame->uniformBuffer,
		.range = sizeof(gpu_frame_uniforms_t),
	};
	const VkDescriptorBufferInfo lightBuffer = {
		.buffer = frame->lightBuffer,
		.range = VK_WHOLE_SIZE,
	};
	const VkDescriptorImageInfo spotLightAtlas = {
		.sampler = rc->vulkan->pointClampSampler,
		.imageView = rc->rt->spotLightAtlas.view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	{
		const VkImageMemoryBarrier imageBarriers[] = {
			{
				// Spot Light Atlas -> Attachment Write
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.image = rc->rt->spotLightAtlas.image,
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
#if 0
	{
		const VkRenderingAttachmentInfo depthAttachment = {
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = rc->rt->spotLightAtlas.view,
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

		// for (size_t i = 0; i < gpuSpotLightCount; ++i) {
		// 	frame->spotShadowDraws[i].vertexCount	= worldgenInfo.vertexCount;
		// 	frame->spotShadowDraws[i].instanceCount	= 1;
		// 	frame->spotShadowDraws[i].firstVertex	= 0;
		// 	frame->spotShadowDraws[i].firstInstance	= i;
		// }

		SetViewportAndScissor(cb, (VkOffset2D){0, 0}, (VkExtent2D){SPOT_LIGHT_RESOLUTION, SPOT_LIGHT_RESOLUTION});
		
		// const VkDeviceSize vertexBufferOffset = 0;
		// vkCmdBindVertexBuffers(cb, 0, 1, &worldgenInfo.vertexBuffer, &vertexBufferOffset);
		// vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, worldgenInfo.pipelineLayout, 0, 1, &worldDescriptorSet, 0, NULL);
		// vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, worldgenInfo.shadowPipeline);
		// vkCmdDrawIndirect(cb, frame->spotShadowDrawBuffer, 0, gpuSpotLightCount, sizeof(VkDrawIndirectCommand));

		vkCmdEndRendering(cb);
	}
#endif
	{
		const VkImageMemoryBarrier imageBarriers[] = {
			{
				// Spot Light Atlas -> Shader Read
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.image = rc->rt->spotLightAtlas.image,
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
				.image = rc->rt->sceneColor.image,
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
				.imageView = rc->rt->sceneColor.view,
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				// .resolveImageView = backbufferView,
				// .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				// .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			}};
		const VkRenderingAttachmentInfo depthAttachment = {
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = rc->rt->sceneDepth.view,
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
				.extent = rc->rt->resolution,
			}};
		vkCmdBeginRendering(cb, &renderingInfo);
		{
			SetViewportAndScissor(cb, (VkOffset2D){}, rc->rt->resolution);
			
			model_loader_info_t modelLoaderInfo;
			model_loader_get_info(&modelLoaderInfo, rc->modelLoader);

			if (gpuDrawCount > 0)
			{
				descriptor_allocator_begin(rc->dsalloc, scene->modelDescriptorSetLayout, "SceneModel");
				descriptor_allocator_set_uniform_buffer(rc->dsalloc, 0, frameUniformBuffer);
				descriptor_allocator_set_storage_buffer(rc->dsalloc, 1, (VkDescriptorBufferInfo){ frame->drawBuffer, 0, gpuDrawCount * sizeof(gpu_draw_t) });
				descriptor_allocator_set_storage_buffer(rc->dsalloc, 2, (VkDescriptorBufferInfo){ modelLoaderInfo.storageBuffer, 0, modelLoaderInfo.storageBufferSize });
				const VkDescriptorSet descriptorSet = descriptor_allocator_end(rc->dsalloc);

				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->modelPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
				vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->modelPipeline);
				vkCmdBindIndexBuffer(cb, modelLoaderInfo.storageBuffer, 0, VK_INDEX_TYPE_UINT16);
				vkCmdDrawIndexedIndirect(cb, frame->drawBuffer, 0, gpuDrawCount, sizeof(gpu_draw_t));
			}
			
			FlushDebugRenderer(
				cb, 
				rc->stagingMemory, 
				debugRenderer,
				rc->dsalloc,
				frameUniformBuffer,
				rc->frameIndex);
		}
		vkCmdEndRendering(cb);
	}
}