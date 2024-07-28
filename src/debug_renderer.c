#include "debug_renderer.h"
#include "util.h"
#include "shaders.h"
#include "staging_memory.h"
#include "render_targets.h"

#include <memory.h>
#include <assert.h>
#include <stdio.h>
#include <malloc.h>

//#define MAX_POINTS		(1024 * 1024) // 1M
//#define MAX_LINES		(1024 * 1024) // 1M
//#define MAX_TRIANGLES	(1024 * 1024) // 1M

//#define FRAME_STAGING_MEMORY_SIZE (MAX_POINTS * sizeof(debug_vertex_t) + MAX_LINES * 2 * sizeof(debug_vertex_t) + MAX_TRIANGLES * 3 * sizeof(debug_vertex_t))

#define MAX_FRAME_VERTICES	(1024 * 1024)

static int CreateDebugPipeline(VkPipeline* pipeline, vulkan_t* vulkan, VkPipelineLayout pipelineLayout, VkPrimitiveTopology topology);

int debug_renderer_create(debug_renderer_t* debugRenderer, vulkan_t* vulkan, const debug_renderer_config_t* config)
{
	int r;

	const VkDescriptorSetLayoutBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
	};
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = countof(bindings),
		.pBindings = bindings,
	};
	if (vkCreateDescriptorSetLayout(vulkan->device, &descriptorSetLayoutInfo, NULL, &debugRenderer->descriptorSetLayout) != VK_SUCCESS) {
		return 1;
	}
	SetDescriptorSetLayoutName(vulkan, debugRenderer->descriptorSetLayout, "DebugRenderer");

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &debugRenderer->descriptorSetLayout,
	};
	if (vkCreatePipelineLayout(vulkan->device, &pipelineLayoutInfo, NULL, &debugRenderer->pipelineLayout) != VK_SUCCESS) {
		return 1;
	}
	SetPipelineLayoutName(vulkan, debugRenderer->pipelineLayout, "DebugRenderer");

	debugRenderer->maxPoints			= config->maxPoints;
	debugRenderer->maxLines				= config->maxLines;
	debugRenderer->maxTriangles			= config->maxTriangles;

	r = CreateDebugPipeline(&debugRenderer->pointPipeline, vulkan, debugRenderer->pipelineLayout, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	assert(r == 0);
	r = CreateDebugPipeline(&debugRenderer->linePipeline, vulkan, debugRenderer->pipelineLayout, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	assert(r == 0);
	r = CreateDebugPipeline(&debugRenderer->trianglePipeline, vulkan, debugRenderer->pipelineLayout, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	assert(r == 0);

	debugRenderer->pointVertices = calloc(config->maxPoints, sizeof(debug_vertex_t));
	debugRenderer->lineVertices = calloc(config->maxLines * 2, sizeof(debug_vertex_t));
	debugRenderer->triangleVertices = calloc(config->maxTriangles * 3, sizeof(debug_vertex_t));

	return 0;
}

void debug_renderer_destroy(debug_renderer_t* debugRenderer, vulkan_t* vulkan)
{
	for (size_t i = 0; i < FRAME_COUNT; ++i) {
		debug_renderer_frame_t* frame = &debugRenderer->frames[i];
		vkDestroyBuffer(vulkan->device, frame->vertexBuffer, NULL);
	}

	vkDestroyPipeline(vulkan->device, debugRenderer->pointPipeline, NULL);
	vkDestroyPipeline(vulkan->device, debugRenderer->linePipeline, NULL);
	vkDestroyPipeline(vulkan->device, debugRenderer->trianglePipeline, NULL);
	vkDestroyPipelineLayout(vulkan->device, debugRenderer->pipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(vulkan->device, debugRenderer->descriptorSetLayout, NULL);
}

int AllocateDebugRendererStagingMemory(staging_memory_allocator_t* allocator, debug_renderer_t* debugRenderer)
{
	for (size_t i = 0; i < FRAME_COUNT; ++i) {
		debug_renderer_frame_t* frame = &debugRenderer->frames[i];
		PushStagingBufferAllocation(allocator, &frame->vertexBuffer, (void**)&frame->vertices, MAX_FRAME_VERTICES * sizeof(debug_vertex_t), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "Debug Vertices");
	}

	return 0;
}

void FlushDebugRenderer(
	VkCommandBuffer cb, 
	staging_memory_context_t* stagingMemory,
	debug_renderer_t* debugRenderer,
	descriptor_allocator_t* dsalloc,
	VkDescriptorBufferInfo frameUniformBuffer,
	uint32_t frameIndex)
{
	const size_t totalVertexCount = debugRenderer->pointCount + debugRenderer->lineCount * 2 + debugRenderer->triangleCount * 3;
	if (totalVertexCount == 0) {
		return;
	}

	debug_renderer_frame_t* frame = &debugRenderer->frames[frameIndex];
	
	const size_t pointVertexOffset = 0u;
	const size_t lineVertexOffset = debugRenderer->pointCount;
	const size_t triangleVertexOffset = debugRenderer->pointCount + debugRenderer->lineCount * 2;

	memcpy(frame->vertices + pointVertexOffset, debugRenderer->pointVertices, debugRenderer->pointCount * sizeof(debug_vertex_t));
	memcpy(frame->vertices + lineVertexOffset, debugRenderer->lineVertices, debugRenderer->lineCount * 2 * sizeof(debug_vertex_t));
	memcpy(frame->vertices + triangleVertexOffset, debugRenderer->triangleVertices, debugRenderer->triangleCount * 3 * sizeof(debug_vertex_t));

	const VkDeviceSize pointBufferOffset = pointVertexOffset * sizeof(debug_vertex_t);
	const VkDeviceSize lineBufferOffset = lineVertexOffset * sizeof(debug_vertex_t);
	const VkDeviceSize triangleBufferOffset = triangleVertexOffset * sizeof(debug_vertex_t);

	descriptor_allocator_begin(dsalloc, debugRenderer->descriptorSetLayout, "Debug Renderer");
	descriptor_allocator_set_uniform_buffer(dsalloc, 0, frameUniformBuffer);
	VkDescriptorSet descriptorSet = descriptor_allocator_end(dsalloc);

	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, debugRenderer->pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

	if (debugRenderer->pointCount > 0) {
		vkCmdBindVertexBuffers(cb, 0, 1, &frame->vertexBuffer, &pointBufferOffset);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, debugRenderer->pointPipeline);
		vkCmdDraw(cb, debugRenderer->pointCount, 1, 0, 0);
	}
	if (debugRenderer->lineCount > 0) {
		vkCmdBindVertexBuffers(cb, 0, 1, &frame->vertexBuffer, &lineBufferOffset);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, debugRenderer->linePipeline);
		vkCmdDraw(cb, debugRenderer->lineCount * 2, 1, 0, 0);
	}
	if (debugRenderer->triangleCount > 0) {
		vkCmdBindVertexBuffers(cb, 0, 1, &frame->vertexBuffer, &triangleBufferOffset);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, debugRenderer->trianglePipeline);
		vkCmdDraw(cb, debugRenderer->triangleCount * 3, 1, 0, 0);
	}

	debugRenderer->pointCount = 0;
	debugRenderer->lineCount = 0;
	debugRenderer->triangleCount = 0;

	PushStagingMemoryFlush(stagingMemory, frame->vertices, totalVertexCount * sizeof(debug_vertex_t));
}

static debug_renderer_t* g_debugRenderer;

void MakeCurrentDebugRenderer(debug_renderer_t* debugRenderer)
{
	g_debugRenderer = debugRenderer;
}

void DrawDebugPoint(debug_vertex_t vertex)
{
	debug_renderer_t* debugRenderer = g_debugRenderer;
	if (debugRenderer->pointCount >= debugRenderer->maxPoints) {
		return;
	}
	
	debugRenderer->pointVertices[debugRenderer->pointCount++] = vertex;
}

void DrawDebugLine(debug_vertex_t v0, debug_vertex_t v1)
{
	debug_renderer_t* debugRenderer = g_debugRenderer;
	if (debugRenderer->lineCount >= debugRenderer->maxLines) {
		return;
	}

	const size_t offset = debugRenderer->lineCount * 2;
	debugRenderer->lineVertices[offset + 0u] = v0;
	debugRenderer->lineVertices[offset + 1u] = v1;
	++debugRenderer->lineCount;
}

void DrawDebugTriangle(debug_vertex_t v0, debug_vertex_t v1, debug_vertex_t v2)
{
	debug_renderer_t* debugRenderer = g_debugRenderer;
	if (debugRenderer->triangleCount >= debugRenderer->maxTriangles) {
		return;
	}

	const size_t offset = debugRenderer->triangleCount * 3;
	debugRenderer->triangleVertices[offset + 0u] = v0;
	debugRenderer->triangleVertices[offset + 1u] = v1;
	debugRenderer->triangleVertices[offset + 2u] = v2;
	++debugRenderer->triangleCount;
}

void DrawDebugBox(vec3 a, vec3 b, uint32_t color)
{
	DrawDebugLine((debug_vertex_t){a.x, a.y, a.z, color}, (debug_vertex_t){b.x, a.y, a.z, color});
	DrawDebugLine((debug_vertex_t){a.x, b.y, a.z, color}, (debug_vertex_t){b.x, b.y, a.z, color});
	DrawDebugLine((debug_vertex_t){a.x, a.y, b.z, color}, (debug_vertex_t){b.x, a.y, b.z, color});
	DrawDebugLine((debug_vertex_t){a.x, b.y, b.z, color}, (debug_vertex_t){b.x, b.y, b.z, color});

	DrawDebugLine((debug_vertex_t){a.x, a.y, a.z, color}, (debug_vertex_t){a.x, b.y, a.z, color});
	DrawDebugLine((debug_vertex_t){b.x, a.y, a.z, color}, (debug_vertex_t){b.x, b.y, a.z, color});
	DrawDebugLine((debug_vertex_t){a.x, a.y, b.z, color}, (debug_vertex_t){a.x, b.y, b.z, color});
	DrawDebugLine((debug_vertex_t){b.x, a.y, b.z, color}, (debug_vertex_t){b.x, b.y, b.z, color});

	DrawDebugLine((debug_vertex_t){a.x, a.y, a.z, color}, (debug_vertex_t){a.x, a.y, b.z, color});
	DrawDebugLine((debug_vertex_t){b.x, a.y, a.z, color}, (debug_vertex_t){b.x, a.y, b.z, color});
	DrawDebugLine((debug_vertex_t){a.x, b.y, a.z, color}, (debug_vertex_t){a.x, b.y, b.z, color});
	DrawDebugLine((debug_vertex_t){b.x, b.y, a.z, color}, (debug_vertex_t){b.x, b.y, b.z, color});
}

void DrawDebugCross(vec3 p, float size, uint32_t color)
{
	const float e = size * 0.5f;
	DrawDebugLine((debug_vertex_t){p.x - e, p.y, p.z, color}, (debug_vertex_t){p.x + e, p.y, p.z, color});
	DrawDebugLine((debug_vertex_t){p.x, p.y - e, p.z, color}, (debug_vertex_t){p.x, p.y + e, p.z, color});
	DrawDebugLine((debug_vertex_t){p.x, p.y, p.z - e, color}, (debug_vertex_t){p.x, p.y, p.z + e, color});
}

static int CreateDebugPipeline(
	VkPipeline* pipeline, 
	vulkan_t* vulkan, 
	VkPipelineLayout pipelineLayout, 
	VkPrimitiveTopology topology)
{
	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = g_shaders.modules[SHADER_DEBUG_VERT],
			.pName = "main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = g_shaders.modules[SHADER_DEBUG_FRAG],
			.pName = "main",
		},
	};

	const VkVertexInputBindingDescription vertexBindings[] = {
		{
			.binding = 0,
			.stride = sizeof(debug_vertex_t),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		}
	};

	const VkVertexInputAttributeDescription vertexAttributes[] = {
		{
			.binding = 0,
			.location = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
		},
		{
			.binding = 0,
			.location = 1,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.offset = offsetof(debug_vertex_t, color),
		}
	};

	const VkPipelineVertexInputStateCreateInfo vertexInput = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = countof(vertexBindings),
		.pVertexBindingDescriptions = vertexBindings,
		.vertexAttributeDescriptionCount = countof(vertexAttributes),
		.pVertexAttributeDescriptions = vertexAttributes,
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssembler = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = topology,
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
		.layout = pipelineLayout,
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembler,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pColorBlendState = &colorBlending,
		.pViewportState = &viewportState,
		.pDynamicState = &dynamicState,
		.pDepthStencilState = &depthStencilState,
	};

	if (vkCreateGraphicsPipelines(vulkan->device, NULL, 1, &createInfo, NULL, pipeline) != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines failed\n");
		return 1;
	}

	return 0;
}