#include "painter.h"
#include "types.h"
#include "rng.h"
#include "shaders.h"
#include "common.h"
#include "util.h"
#include "render_targets.h"

#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <malloc.h>
#include <math.h>

#define PAINTING_SIZE 64
#define PAINTING_ATLAS_SIZE 4
#define PAINTING_COUNT (PAINTING_ATLAS_SIZE * PAINTING_ATLAS_SIZE)
#define MAX_PAINTING_TRIANGLES	(256)
#define MAX_PAINTING_POINTS		(256)
#define MAX_PAINTING_VERTICES	(MAX_PAINTING_TRIANGLES*3 + MAX_PAINTING_POINTS)
#define PAINTING_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT

typedef struct painting_vertex {
	float x, y;
	uint32_t color;
	float size;
} painting_vertex_t;

enum {
	Painting_Init = 0,
	Painting_Draw,
	Painting_Render,
	Painting_RenderAwait,
	Painting_Done,
};

typedef struct painting {
	uint8_t		state;
	uint64_t	frameId;
	uint32_t	triangleVertexCount;
	uint32_t	pointVertexCount;
} painting_t;

typedef struct painter {
	vulkan_t*				vulkan;
	bool					isFirstTick;
	uint32_t				rng;

	dedicated_render_target_t	rt;

	VkDescriptorSetLayout	descriptorSetLayout;
	VkPipelineLayout		pipelineLayout;
	VkPipeline				trianglePipeline;
	VkPipeline				pointPipeline;

	painting_t				paintings[PAINTING_COUNT];
	uint16_t				renderPainting;

	VkBuffer				vertexBuffer;
	painting_vertex_t*		vertices;
	bool					vertexLock;
} painter_t;

static void CreatePipelineLayout(
	painter_t* painter,
	vulkan_t* vulkan);

static int CreatePipelines(
	painter_t* painter, 
	vulkan_t* vulkan);

painter_t* CreatePainter(
	vulkan_t* vulkan)
{
	painter_t* painter = calloc(1, sizeof(painter_t));
	if (painter == NULL) {
		return NULL;
	}

	painter->vulkan			= vulkan;
	painter->isFirstTick	= true;

	painter->rng = time(NULL);
	lcg_rand(&painter->rng);
	lcg_rand(&painter->rng);
	lcg_rand(&painter->rng);
	
	CreatePipelineLayout(painter, vulkan);
	CreatePipelines(painter, vulkan);

	AllocateDedicatedRenderTarget2D(
		&painter->rt, 
		vulkan, 
		(VkExtent2D){PAINTING_SIZE * PAINTING_ATLAS_SIZE, PAINTING_SIZE * PAINTING_ATLAS_SIZE},
		PAINTING_FORMAT, 
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
		"Paintings");

	return painter;
}

int AllocatePainterStagingMemory(
	staging_memory_allocator_t* allocator,
	painter_t* painter)
{
	PushStagingBufferAllocation(allocator, &painter->vertexBuffer, (void**)&painter->vertices, MAX_PAINTING_VERTICES * sizeof(painting_vertex_t), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "Painter Vertices");
}

typedef struct paint_context {
	size_t				triangleCount;
	painting_vertex_t*	triangleVertices;
	size_t				pointCount;
	painting_vertex_t*	pointVertices;
} paint_context_t;

static float lcg_randf_ndc(uint32_t* rng)
{
	return lcg_randf(rng) * 2.0f - 1.0f;
}

static painting_vertex_t* DrawTriangles(paint_context_t* ctx, size_t count)
{
	const size_t offset = ctx->triangleCount * 3;
	ctx->triangleCount += count;
	return ctx->triangleVertices + offset;
}

static painting_vertex_t* DrawPoints(paint_context_t* ctx, size_t count)
{
	const size_t offset = ctx->pointCount;
	ctx->pointCount += count;
	return ctx->pointVertices + offset;
}

static const uint32_t bgpalette[] = {
	0x16161616,
	0xeeeeeeee,
};

static const uint32_t splatpalette[] = {
	0xeeeeeeee,
	// 0xff00ff00,
	// 0xffff00ff,
	// 0xff00ffff,
};

static void DrawPainting(paint_context_t* ctx, uint32_t seed)
{
	painting_vertex_t* v;

	uint32_t rng = seed;

	const float A = 1.0f + lcg_randf(&rng) * 2.0f;
	const float B = 1.0f + lcg_randf(&rng) * 2.0f;
	const float C = 1.0f + lcg_randf(&rng) * 2.0f;

	const uint32_t bgcolor = bgpalette[lcg_rand(&rng) % countof(bgpalette)];

	v = DrawTriangles(ctx, 2);

	v[0] = (painting_vertex_t){-1.0f, -1.0f, bgcolor};
	v[1] = (painting_vertex_t){+1.0f, -1.0f, bgcolor};
	v[2] = (painting_vertex_t){-1.0f, +1.0f, bgcolor};

	v[3] = (painting_vertex_t){-1.0f, +1.0f, bgcolor};
	v[4] = (painting_vertex_t){+1.0f, -1.0f, bgcolor};
	v[5] = (painting_vertex_t){+1.0f, +1.0f, bgcolor};
	
	v = DrawTriangles(ctx, 5);
	for (int i = 0; i < 5; ++i)
	{
		const uint32_t color = lcg_rand(&rng);
		v[i*3 + 0] = (painting_vertex_t){ lcg_randf_ndc(&rng) * C, lcg_randf_ndc(&rng) * B, color };
		v[i*3 + 1] = (painting_vertex_t){ lcg_randf_ndc(&rng) * B, lcg_randf_ndc(&rng) * A, color };
		v[i*3 + 2] = (painting_vertex_t){ lcg_randf_ndc(&rng) * A, lcg_randf_ndc(&rng) * C, color };
	}

	const float Q = lcg_randf(&rng) * 3.14f;
	const float W = lcg_randf(&rng) * 3.14f * 2.0f + 1.2f;
	const float E = lcg_randf(&rng) * 0.8f + 0.3f;
	const float R = lcg_randf(&rng) * 0.8f + 0.3f;
	const float T = lcg_randf(&rng) * 2.0f + 1.0f;

	const uint32_t splatcolor = splatpalette[lcg_rand(&rng) % countof(splatpalette)];

	v = DrawPoints(ctx, 80);
	for (int i = 0; i < 80; ++i)
	{
		const float t = Q + W * lcg_randf(&rng);
		const float s = E * sinf(t);
		const float c = R * cosf(t);
		const float size = (sinf(t * 5.0f + W) * 0.5f + 0.5f) * T + 1.0f;

		v[i] = (painting_vertex_t){ s + c*lcg_randf(&rng), c + s*lcg_randf(&rng), splatcolor, size };
	}
}

void TickPainter(
	painter_t* painter,
	uint64_t frameId)
{
	for (int i = 0; i < PAINTING_COUNT; ++i) {
		painting_t* painting = &painter->paintings[i];
		if (painting->state == Painting_Init) {
			++painting->state;
		}
		
		if (painting->state == Painting_Draw) {
			if (painter->vertexLock || painter->renderPainting != 0) {
				continue;
			}
			painter->vertexLock = true;

			paint_context_t ctx = { 
				.triangleVertices = painter->vertices,
				.pointVertices = painter->vertices + 3 * MAX_PAINTING_TRIANGLES,
			};

			DrawPainting(&ctx, lcg_rand(&painter->rng));

			painting->triangleVertexCount = 3 * ctx.triangleCount;
			painting->pointVertexCount = ctx.pointCount;

			painter->renderPainting = i + 1;
			++painting->state;
		}

		if (painting->state == Painting_RenderAwait)
		{
			if (frameId - painting->frameId < FRAME_COUNT) {
				continue;
			}

			painter->vertexLock = false;
			//painter->renderPainting = i + 1;
			++painting->state;
		}

		if (painting->state == Painting_Done)
		{
		}
	}
}

static void RenderPainting(
	VkCommandBuffer cb,
	painter_t* painter,
	uint32_t paintingIndex)
{
	painting_t* painting = painter->paintings + paintingIndex;
	assert(painting->state == Painting_Render);
	
	const VkOffset2D atlasOffset = { 
		.x = (paintingIndex % PAINTING_ATLAS_SIZE) * PAINTING_SIZE,
		.y = (paintingIndex / PAINTING_ATLAS_SIZE) * PAINTING_SIZE,
	};

	const VkRenderingAttachmentInfo colorAttachments[] = {
		{
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = painter->rt.view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		},
	};

	const VkRenderingInfo renderingInfo = {
		VK_STRUCTURE_TYPE_RENDERING_INFO,
		.colorAttachmentCount = countof(colorAttachments),
		.pColorAttachments = colorAttachments,
		.layerCount = 1,
		.renderArea = { // does this do anything?
			.offset = atlasOffset,
			.extent = { PAINTING_SIZE, PAINTING_SIZE },
		},
	};

	vkCmdBeginRendering(cb, &renderingInfo);

	SetViewportAndScissor(cb, atlasOffset, (VkExtent2D){PAINTING_SIZE, PAINTING_SIZE});

	if (painting->triangleVertexCount > 0)
	{
		const VkDeviceSize vbOffset = 0;
		vkCmdBindVertexBuffers(cb, 0, 1, &painter->vertexBuffer, &vbOffset);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, painter->trianglePipeline);
		vkCmdDraw(cb, painting->triangleVertexCount, 1, 0, 0);
	}
	
	if (painting->pointVertexCount > 0)
	{
		const VkDeviceSize vbOffset = 3 * MAX_PAINTING_TRIANGLES * sizeof(painting_vertex_t);
		vkCmdBindVertexBuffers(cb, 0, 1, &painter->vertexBuffer, &vbOffset);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, painter->pointPipeline);
		vkCmdDraw(cb, painting->pointVertexCount, 1, 0, 0);
	}

	vkCmdEndRendering(cb);
}

void RenderPaintings(
	VkCommandBuffer cb,
	painter_t* painter,
	staging_memory_context_t* staging,
	uint64_t frameId)
{
	const bool isFirstTick = painter->isFirstTick;

	painter->isFirstTick = false;

	if (painter->renderPainting == 0) {
		if (isFirstTick) {
			const VkImageMemoryBarrier imageBarriers[] = {
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.image = painter->rt.image,
					.srcAccessMask = VK_ACCESS_NONE,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.layerCount = 1,
						.levelCount = 1,
					},
				},
			};
			vkCmdPipelineBarrier(cb,
				VK_PIPELINE_STAGE_NONE,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, NULL,
				0, NULL,
				countof(imageBarriers), imageBarriers);
		}
		return;
	}

	if (isFirstTick)
	{
		const VkImageMemoryBarrier imageBarriers[] = {
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.image = painter->rt.image,
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
			VK_PIPELINE_STAGE_NONE,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0,
			0, NULL,
			0, NULL,
			countof(imageBarriers), imageBarriers);
	}
	else
	{
		const VkImageMemoryBarrier imageBarriers[] = {
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.image = painter->rt.image,
				.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.layerCount = 1,
					.levelCount = 1,
				},
			},
		};
		vkCmdPipelineBarrier(cb,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0,
			0, NULL,
			0, NULL,
			countof(imageBarriers), imageBarriers);
	}

	const uint16_t paintingIndex = painter->renderPainting - 1;
	painting_t* painting = &painter->paintings[paintingIndex];

	RenderPainting(
		cb,
		painter,
		paintingIndex);

	{
		const VkImageMemoryBarrier imageBarriers[] = {
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.image = painter->rt.image,
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
		};
		vkCmdPipelineBarrier(cb,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, NULL,
			0, NULL,
			countof(imageBarriers), imageBarriers);
	}

	// result->image = (VkDescriptorImageInfo){
	// 	.sampler = vulkan->pointClampSampler,
	// 	.imageView = painter->imageView,
	// 	.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	// };

	++painting->state;
	painting->frameId = frameId;

	painter->renderPainting = 0;
}

static void CreatePipelineLayout(
	painter_t* painter,
	vulkan_t* vulkan)
{
	const VkDescriptorSetLayoutBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
	};
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = countof(bindings),
		.pBindings = bindings,
	};
	if (vkCreateDescriptorSetLayout(vulkan->device, &descriptorSetLayoutInfo, NULL, &painter->descriptorSetLayout) != VK_SUCCESS) {
		return;
	}
	SetDescriptorSetLayoutName(vulkan, painter->descriptorSetLayout, "Painter");

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		// .setLayoutCount = 1,
		// .pSetLayouts = &painter->descriptorSetLayout,
	};
	if (vkCreatePipelineLayout(vulkan->device, &pipelineLayoutInfo, NULL, &painter->pipelineLayout) != VK_SUCCESS) {
		return;
	}
	SetPipelineLayoutName(vulkan, painter->pipelineLayout, "Painter");

	return;
}

static int CreatePainterPipeline(
	VkPipeline* pipeline,
	VkDevice device,
	VkPipelineLayout pipelineLayout,
	VkPrimitiveTopology topology)
{
	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = g_shaders.modules[SHADER_PAINTER_VERT],
			.pName = "main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = g_shaders.modules[SHADER_PAINTER_FRAG],
			.pName = "main",
		},
	};

	const VkVertexInputBindingDescription vertexBindings[] = {
		{
			.binding = 0,
			.stride = sizeof(painting_vertex_t),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		}
	};

	const VkVertexInputAttributeDescription vertexAttributes[] = {
		{
			.binding = 0,
			.location = 0,
			.format = VK_FORMAT_R32G32_SFLOAT,
		},
		{
			.binding = 0,
			.location = 1,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.offset = offsetof(painting_vertex_t, color),
		},
		{
			.binding = 0,
			.location = 2,
			.format = VK_FORMAT_R32_SFLOAT,
			.offset = offsetof(painting_vertex_t, size),
		},
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
		// .depthTestEnable = VK_TRUE,
		// .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		// .depthWriteEnable = VK_TRUE,
	};

	const VkFormat colorFormats[] = { PAINTING_FORMAT };
	const VkPipelineRenderingCreateInfo renderingInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = countof(colorFormats),
		.pColorAttachmentFormats = colorFormats,
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

	if (vkCreateGraphicsPipelines(device, NULL, 1, &createInfo, NULL, pipeline) != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines failed\n");
		return 1;
	}
}

static int CreatePipelines(
	painter_t* painter, 
	vulkan_t* vulkan)
{
	CreatePainterPipeline(&painter->trianglePipeline, vulkan->device, painter->pipelineLayout, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	CreatePainterPipeline(&painter->pointPipeline, vulkan->device, painter->pipelineLayout, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

	return 0;
}

VkDescriptorImageInfo GetPaintingImage(
	painter_t* painter)
{
	return (VkDescriptorImageInfo){
		.sampler = painter->vulkan->pointClampSampler,
		.imageView = painter->rt.view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
}