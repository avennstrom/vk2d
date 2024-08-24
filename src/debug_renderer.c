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

typedef struct debug_renderer_frame {
	VkBuffer		vertexBuffer;
	debug_vertex_t*	vertices;
} debug_renderer_frame_t;

typedef struct debug_renderer_buffer
{
	size_t					maxPoints;
	size_t					maxLines;
	size_t					maxTriangles;

	debug_vertex_t*			pointVertices;
	size_t					pointCount;
	debug_vertex_t*			lineVertices;
	size_t					lineCount;
	debug_vertex_t*			triangleVertices;
	size_t					triangleCount;
} debug_renderer_buffer_t;

typedef struct debug_renderer
{
	debug_renderer_frame_t	frames[FRAME_COUNT];
	
	VkDescriptorSetLayout	descriptorSetLayout;
	VkPipelineLayout		pipelineLayout;
	VkPipeline				pointPipeline;
	VkPipeline				linePipeline;
	VkPipeline				trianglePipeline;

	// draw state
	debug_renderer_buffer_t	buffers[DEBUG_RENDERER_BUFFER_COUNT];
} debug_renderer_t;

static int CreateDebugPipeline(VkPipeline* pipeline, vulkan_t* vulkan, VkPipelineLayout pipelineLayout, VkPrimitiveTopology topology);

static int debug_renderer_buffer_alloc(debug_renderer_buffer_t* buffer, const debug_renderer_config_t* config)
{
	buffer->maxPoints			= config->maxPoints;
	buffer->maxLines			= config->maxLines;
	buffer->maxTriangles		= config->maxTriangles;

	buffer->pointVertices = calloc(config->maxPoints, sizeof(debug_vertex_t));
	buffer->lineVertices = calloc(config->maxLines * 2, sizeof(debug_vertex_t));
	buffer->triangleVertices = calloc(config->maxTriangles * 3, sizeof(debug_vertex_t));
}

debug_renderer_t* debug_renderer_create(vulkan_t* vulkan, const debug_renderer_config_t* config)
{
	int r;

	debug_renderer_t* debugRenderer = calloc(1, sizeof(debug_renderer_t));
	if (debugRenderer == NULL)
	{
		return NULL;
	}

	const VkDescriptorSetLayoutBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
	};
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = countof(bindings),
		.pBindings = bindings,
	};
	if (vkCreateDescriptorSetLayout(vulkan->device, &descriptorSetLayoutInfo, NULL, &debugRenderer->descriptorSetLayout) != VK_SUCCESS) {
		return NULL;
	}
	SetDescriptorSetLayoutName(vulkan, debugRenderer->descriptorSetLayout, "DebugRenderer");

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &debugRenderer->descriptorSetLayout,
	};
	if (vkCreatePipelineLayout(vulkan->device, &pipelineLayoutInfo, NULL, &debugRenderer->pipelineLayout) != VK_SUCCESS) {
		return NULL;
	}
	SetPipelineLayoutName(vulkan, debugRenderer->pipelineLayout, "DebugRenderer");

	r = CreateDebugPipeline(&debugRenderer->pointPipeline, vulkan, debugRenderer->pipelineLayout, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	assert(r == 0);
	r = CreateDebugPipeline(&debugRenderer->linePipeline, vulkan, debugRenderer->pipelineLayout, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	assert(r == 0);
	r = CreateDebugPipeline(&debugRenderer->trianglePipeline, vulkan, debugRenderer->pipelineLayout, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	assert(r == 0);
	
	for (int i = 0; i < DEBUG_RENDERER_BUFFER_COUNT; ++i)
	{
		debug_renderer_buffer_alloc(&debugRenderer->buffers[i], config);
	}

	return debugRenderer;
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

void debug_renderer_flush(
	VkCommandBuffer cb, 
	staging_memory_context_t* stagingMemory,
	debug_renderer_t* debugRenderer,
	descriptor_allocator_t* dsalloc,
	VkDescriptorBufferInfo frameUniformBuffer,
	uint32_t frameIndex)
{	
	size_t totalVertexCount = 0u;

	size_t pointCount = 0u;
	size_t lineCount = 0u;
	size_t triangleCount = 0u;

	for (int bufferIndex = 0; bufferIndex < DEBUG_RENDERER_BUFFER_COUNT; ++bufferIndex)
	{
		debug_renderer_buffer_t* buffer = &debugRenderer->buffers[bufferIndex];
		
		totalVertexCount += buffer->pointCount + buffer->lineCount * 2 + buffer->triangleCount * 3;
		
		pointCount += buffer->pointCount;
		lineCount += buffer->lineCount;
		triangleCount += buffer->triangleCount;
	}

	if (totalVertexCount == 0) {
		return;
	}

	debug_renderer_frame_t* frame = &debugRenderer->frames[frameIndex];
	
	const size_t pointVertexOffset = 0u;
	const size_t lineVertexOffset = pointCount;
	const size_t triangleVertexOffset = pointCount + lineCount * 2;

	debug_vertex_t* pointVertices = frame->vertices + pointVertexOffset;
	debug_vertex_t* lineVertices = frame->vertices + lineVertexOffset;
	debug_vertex_t* triangleVertices = frame->vertices + triangleVertexOffset;
	
	for (int bufferIndex = 0; bufferIndex < DEBUG_RENDERER_BUFFER_COUNT; ++bufferIndex)
	{
		debug_renderer_buffer_t* buffer = &debugRenderer->buffers[bufferIndex];
		
		memcpy(pointVertices, buffer->pointVertices, buffer->pointCount * sizeof(debug_vertex_t));
		memcpy(lineVertices, buffer->lineVertices, buffer->lineCount * 2 * sizeof(debug_vertex_t));
		memcpy(triangleVertices, buffer->triangleVertices, buffer->pointCount * 3 * sizeof(debug_vertex_t));
		
		pointVertices += buffer->pointCount;
		lineVertices += buffer->lineCount * 2;
		triangleVertices += buffer->triangleCount * 3;
	}

	const VkDeviceSize pointBufferOffset = pointVertexOffset * sizeof(debug_vertex_t);
	const VkDeviceSize lineBufferOffset = lineVertexOffset * sizeof(debug_vertex_t);
	const VkDeviceSize triangleBufferOffset = triangleVertexOffset * sizeof(debug_vertex_t);

	descriptor_allocator_begin(dsalloc, debugRenderer->descriptorSetLayout, "Debug Renderer");
	descriptor_allocator_set_uniform_buffer(dsalloc, 0, frameUniformBuffer);
	VkDescriptorSet descriptorSet = descriptor_allocator_end(dsalloc);

	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, debugRenderer->pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

	if (pointCount > 0) {
		vkCmdBindVertexBuffers(cb, 0, 1, &frame->vertexBuffer, &pointBufferOffset);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, debugRenderer->pointPipeline);
		vkCmdDraw(cb, pointCount, 1, 0, 0);
	}
	if (lineCount > 0) {
		vkCmdBindVertexBuffers(cb, 0, 1, &frame->vertexBuffer, &lineBufferOffset);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, debugRenderer->linePipeline);
		vkCmdDraw(cb, lineCount * 2, 1, 0, 0);
	}
	if (triangleCount > 0) {
		vkCmdBindVertexBuffers(cb, 0, 1, &frame->vertexBuffer, &triangleBufferOffset);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, debugRenderer->trianglePipeline);
		vkCmdDraw(cb, triangleCount * 3, 1, 0, 0);
	}

	PushStagingMemoryFlush(stagingMemory, frame->vertices, totalVertexCount * sizeof(debug_vertex_t));
}

static debug_renderer_buffer_t* g_currentBuffer;

void debug_renderer_clear_buffer(debug_renderer_t* debugRenderer, debug_renderer_buffer_id_t id)
{
	debug_renderer_buffer_t* buffer = &debugRenderer->buffers[id];
	buffer->pointCount = 0;
	buffer->lineCount = 0;
	buffer->triangleCount = 0;
}

void debug_renderer_set_current_buffer(debug_renderer_t* debugRenderer, debug_renderer_buffer_id_t id)
{
	g_currentBuffer = &debugRenderer->buffers[id];
}

void DrawDebugPoint(debug_vertex_t vertex)
{
	debug_renderer_buffer_t* buffer = g_currentBuffer;
	if (buffer->pointCount >= buffer->maxPoints) {
		return;
	}
	
	buffer->pointVertices[buffer->pointCount++] = vertex;
}

void DrawDebugLine(debug_vertex_t v0, debug_vertex_t v1)
{
	debug_renderer_buffer_t* buffer = g_currentBuffer;
	if (buffer->lineCount >= buffer->maxLines) {
		return;
	}

	const size_t offset = buffer->lineCount * 2;
	buffer->lineVertices[offset + 0u] = v0;
	buffer->lineVertices[offset + 1u] = v1;
	++buffer->lineCount;
}

void DrawDebugTriangle(debug_vertex_t v0, debug_vertex_t v1, debug_vertex_t v2)
{
	debug_renderer_buffer_t* buffer = g_currentBuffer;
	if (buffer->triangleCount >= buffer->maxTriangles) {
		return;
	}

	const size_t offset = buffer->triangleCount * 3;
	buffer->triangleVertices[offset + 0u] = v0;
	buffer->triangleVertices[offset + 1u] = v1;
	buffer->triangleVertices[offset + 2u] = v2;
	++buffer->triangleCount;
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
			.pName = "vs_main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = g_shaders.modules[SHADER_DEBUG_FRAG],
			.pName = "fs_main",
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
		.lineWidth = 4.0f,
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