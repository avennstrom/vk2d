#include "scene.h"
#include "common.h"
#include "util.h"
#include "vulkan.h"
#include "mat.h"
#include "color.h"
#include "terrain.h"
#include "world.h"

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

	VkDescriptorSetLayout	worldDescriptorSetLayout;
	VkPipelineLayout		worldPipelineLayout;
	VkPipeline				worldPipeline;

	// VkDescriptorSetLayout	terrainDescriptorSetLayout;
	// VkPipelineLayout		terrainPipelineLayout;
	// VkPipeline				terrainPipeline;

	scene_frame_t	frames[FRAME_COUNT];
} scene_t;

static int scene_create_model_pipeline(scene_t* scene, vulkan_t* vulkan)
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
	SetDescriptorSetLayoutName(vulkan, scene->modelDescriptorSetLayout, "Model");

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &scene->modelDescriptorSetLayout,
	};
	if (vkCreatePipelineLayout(vulkan->device, &pipelineLayoutInfo, NULL, &scene->modelPipelineLayout) != VK_SUCCESS) {
		return 1;
	}
	SetPipelineLayoutName(vulkan, scene->modelPipelineLayout, "Model");
	
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

	const VkPipelineVertexInputStateCreateInfo vertexInput = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
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
	SetPipelineName(vulkan, scene->modelPipeline, "Model");
}

static int scene_create_world_pipeline(scene_t* scene, vulkan_t* vulkan)
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
	if (vkCreateDescriptorSetLayout(vulkan->device, &descriptorSetLayoutInfo, NULL, &scene->worldDescriptorSetLayout) != VK_SUCCESS) {
		return 1;
	}
	SetDescriptorSetLayoutName(vulkan, scene->worldDescriptorSetLayout, "World");

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &scene->worldDescriptorSetLayout,
	};
	if (vkCreatePipelineLayout(vulkan->device, &pipelineLayoutInfo, NULL, &scene->worldPipelineLayout) != VK_SUCCESS) {
		return 1;
	}
	SetPipelineLayoutName(vulkan, scene->worldPipelineLayout, "World");
	
	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = g_shaders.modules[SHADER_WORLD_VERT],
			.pName = "vs_main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = g_shaders.modules[SHADER_WORLD_FRAG],
			.pName = "fs_main",
		},
	};

	const VkPipelineVertexInputStateCreateInfo vertexInput = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
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
		.layout = scene->worldPipelineLayout,
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembler,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pColorBlendState = &colorBlending,
		.pViewportState = &viewportState,
		.pDynamicState = &dynamicState,
		.pDepthStencilState = &depthStencilState,
	};

	if (vkCreateGraphicsPipelines(vulkan->device, NULL, 1, &createInfo, NULL, &scene->worldPipeline) != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines failed\n");
		return 1;
	}
	SetPipelineName(vulkan, scene->worldPipeline, "World");
}

#if 0
static int scene_create_terrain_pipeline(scene_t* scene, vulkan_t* vulkan)
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
	if (vkCreateDescriptorSetLayout(vulkan->device, &descriptorSetLayoutInfo, NULL, &scene->terrainDescriptorSetLayout) != VK_SUCCESS) {
		return 1;
	}
	SetDescriptorSetLayoutName(vulkan, scene->terrainDescriptorSetLayout, "Terrain");

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &scene->terrainDescriptorSetLayout,
	};
	if (vkCreatePipelineLayout(vulkan->device, &pipelineLayoutInfo, NULL, &scene->terrainPipelineLayout) != VK_SUCCESS) {
		return 1;
	}
	SetPipelineLayoutName(vulkan, scene->terrainPipelineLayout, "Terrain");
	
	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = g_shaders.modules[SHADER_TERRAIN_VERT],
			.pName = "vs_main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = g_shaders.modules[SHADER_TERRAIN_FRAG],
			.pName = "fs_main",
		},
	};

	const VkPipelineVertexInputStateCreateInfo vertexInput = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
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
		.layout = scene->terrainPipelineLayout,
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembler,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pColorBlendState = &colorBlending,
		.pViewportState = &viewportState,
		.pDynamicState = &dynamicState,
		.pDepthStencilState = &depthStencilState,
	};

	if (vkCreateGraphicsPipelines(vulkan->device, NULL, 1, &createInfo, NULL, &scene->terrainPipeline) != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines failed\n");
		return 1;
	}
	SetPipelineName(vulkan, scene->terrainPipeline, "Terrain");
}
#endif

int scene_alloc_staging_mem(staging_memory_allocator_t* allocator, scene_t *scene)
{
	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		scene_frame_t* frame = &scene->frames[i];

		PushStagingBufferAllocation(allocator, &frame->uniformBuffer, (void**)&frame->uniforms, sizeof(gpu_frame_uniforms_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "Scene Uniforms");
		PushStagingBufferAllocation(allocator, &frame->drawBuffer, (void**)&frame->draws, MAX_DRAWS * sizeof(gpu_draw_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, "Draws");
		//PushStagingBufferAllocation(allocator, &frame->drawCommandBuffer, (void**)&frame->drawCommands, MAX_DRAWS * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Draw Commands");
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

	r = scene_create_model_pipeline(scene, vulkan);
	assert(r == 0);
	r = scene_create_world_pipeline(scene, vulkan);
	assert(r == 0);
	// r = scene_create_terrain_pipeline(scene, vulkan);
	// assert(r == 0);

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
	}
	vkFreeMemory(vulkan->device, scene->stagingMemory, NULL);

	vkDestroyPipeline(vulkan->device, scene->modelPipeline, NULL);
	vkDestroyPipelineLayout(vulkan->device, scene->modelPipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(vulkan->device, scene->modelDescriptorSetLayout, NULL);

	vkDestroyPipeline(vulkan->device, scene->worldPipeline, NULL);
	vkDestroyPipelineLayout(vulkan->device, scene->worldPipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(vulkan->device, scene->worldDescriptorSetLayout, NULL);

#if 0
	vkDestroyPipeline(vulkan->device, scene->terrainPipeline, NULL);
	vkDestroyPipelineLayout(vulkan->device, scene->terrainPipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(vulkan->device, scene->terrainDescriptorSetLayout, NULL);
#endif

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

		gpu_draw_t* gpuDraws = frame->draws;

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
					// for (int partIndex = 0; partIndex < modelInfo.partCount; ++partIndex)
					// {
					// 	const model_part_info_t* partInfo = &modelInfo.parts[partIndex];
					// 	gpuDraws[gpuDrawCount++] = (gpu_draw_t){
					// 		.indexCount = partInfo->indexCount,
					// 		.instanceCount = 1,
					// 		.firstIndex = partInfo->indexOffset,
					// 		.vertexPositionOffset = partInfo->vertexPositionOffset,
					// 		.vertexNormalOffset = partInfo->vertexNormalOffset,
					// 		.vertexColorOffset = partInfo->vertexColorOffset,
					// 		.transform = draws[i].transform[partIndex],
					// 	};
					// }
				}
			}
			break;
		case SCB_CMD_POINT_LIGHT:
			cmdsize = sizeof(scb_point_light_t);
			// scb_point_light_t *pointLights = (scb_point_light_t *)data;
			// for (size_t i = 0; i < header->count; ++i)
			// {
			// 	gpuPointLights[gpuPointLightCount++] = (gpu_point_light_t){
			// 		.pos = pointLights[i].position,
			// 		.radius = pointLights[i].radius,
			// 		.color = pointLights[i].color,
			// 	};
			// }
			break;
		}

		ptr += sizeof(scb_command_header_t) + header->count * cmdsize;
	}

	(*frame->uniforms) = (gpu_frame_uniforms_t){
		.matViewProj		= scb->camera.viewProjectionMatrix,
		.pointLightCount	= gpuPointLightCount,
		.spotLightCount		= gpuSpotLightCount,
		.drawCount			= gpuDrawCount,
		.elapsedTime		= rc->elapsedTime,
	};

	PushStagingMemoryFlush(rc->stagingMemory, frame->uniforms, sizeof(gpu_frame_uniforms_t));

	const VkDescriptorBufferInfo frameUniformBuffer = {
		.buffer = frame->uniformBuffer,
		.range = sizeof(gpu_frame_uniforms_t),
	};
	
	{
		const VkImageMemoryBarrier imageBarriers[] = {
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

	// Scene color pass
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

			world_render_info_t worldInfo;
			if (world_get_render_info(&worldInfo, rc->world))
			{
				descriptor_allocator_begin(rc->dsalloc, scene->worldDescriptorSetLayout, "World");
				descriptor_allocator_set_uniform_buffer(rc->dsalloc, 0, frameUniformBuffer);
				descriptor_allocator_set_storage_buffer(rc->dsalloc, 1, (VkDescriptorBufferInfo){ worldInfo.vertexPositionBuffer, 0, VK_WHOLE_SIZE });
				descriptor_allocator_set_storage_buffer(rc->dsalloc, 2, (VkDescriptorBufferInfo){ worldInfo.vertexColorBuffer, 0, VK_WHOLE_SIZE });
				const VkDescriptorSet descriptorSet = descriptor_allocator_end(rc->dsalloc);

				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->worldPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
				vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, scene->worldPipeline);
				vkCmdBindIndexBuffer(cb, worldInfo.indexBuffer, 0, VK_INDEX_TYPE_UINT16);
				vkCmdDrawIndexed(cb, worldInfo.indexCount, 1, 0, 0, 0);
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