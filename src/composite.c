#include "composite.h"
#include "render_targets.h"
#include "shaders.h"
#include "util.h"
#include "descriptors.h"

#include <assert.h>
#include <malloc.h>

typedef struct composite {
	VkDescriptorSetLayout	descriptorSetLayout;
	VkPipelineLayout		pipelineLayout;
	VkPipeline				pipeline;
} composite_t;

static int create_composite_pipeline_layout(composite_t* composite, vulkan_t* vulkan)
{
	const VkDescriptorSetLayoutBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		{ 1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
	};
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = countof(bindings),
		.pBindings = bindings,
	};
	if (vkCreateDescriptorSetLayout(vulkan->device, &descriptorSetLayoutInfo, NULL, &composite->descriptorSetLayout) != VK_SUCCESS) {
		return 1;
	}
	SetDescriptorSetLayoutName(vulkan, composite->descriptorSetLayout, "Composite");

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &composite->descriptorSetLayout,
	};
	if (vkCreatePipelineLayout(vulkan->device, &pipelineLayoutInfo, NULL, &composite->pipelineLayout) != VK_SUCCESS) {
		return 1;
	}
	SetPipelineLayoutName(vulkan, composite->pipelineLayout, "Composite");

	return 0;
}

static int create_composite_pipeline(composite_t* composite, vulkan_t* vulkan)
{
	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = g_shaders.modules[SHADER_COMPOSITE_VERT],
			.pName = "vs_main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = g_shaders.modules[SHADER_COMPOSITE_FRAG],
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
		.attachmentCount = countof(blendAttachments),
		.pAttachments = blendAttachments,
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencil = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
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

	const VkFormat colorFormats[] = { vulkan->surfaceFormat.format };
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
		.layout = composite->pipelineLayout,
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembler,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pColorBlendState = &colorBlending,
		.pDepthStencilState = &depthStencil,
		.pViewportState = &viewportState,
		.pDynamicState = &dynamicState,
	};

	if (vkCreateGraphicsPipelines(vulkan->device, NULL, 1, &createInfo, NULL, &composite->pipeline) != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines failed\n");
		return 1;
	}

	return 0;
}

composite_t* composite_create(vulkan_t* vulkan)
{
	composite_t* composite = calloc(1, sizeof(composite_t));
	if (composite == NULL) {
		return NULL;
	}

	create_composite_pipeline_layout(composite, vulkan);
	create_composite_pipeline(composite, vulkan);

	return composite;
}

void composite_destroy(composite_t* composite, vulkan_t* vulkan)
{
	vkDestroyPipeline(vulkan->device, composite->pipeline, NULL);
	vkDestroyPipelineLayout(vulkan->device, composite->pipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(vulkan->device, composite->descriptorSetLayout, NULL);
}

void draw_composite(
	VkCommandBuffer cb,
	vulkan_t* vulkan,
	composite_t* composite,
	descriptor_allocator_t* dsalloc,
	render_targets_t* rt,
	VkImageView backbuffer)
{
	descriptor_allocator_begin(dsalloc, composite->descriptorSetLayout, "Composite");
	descriptor_allocator_set_sampled_image(dsalloc, 0, (VkDescriptorImageInfo){ .imageView = rt->sceneColor.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
	descriptor_allocator_set_sampler(dsalloc, 1, vulkan->pointClampSampler);
	VkDescriptorSet descriptorSet = descriptor_allocator_end(dsalloc);

	const VkRenderingAttachmentInfo colorAttachments[] = {
		{
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = backbuffer,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		},
	};

	const VkRenderingInfo renderingInfo = {
		VK_STRUCTURE_TYPE_RENDERING_INFO,
		.colorAttachmentCount = countof(colorAttachments),
		.pColorAttachments = colorAttachments,
		.layerCount = 1,
		.renderArea = {
			.extent = rt->resolution,
		},
	};

	vkCmdBeginRendering(cb, &renderingInfo);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, composite->pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, composite->pipeline);
	vkCmdDraw(cb, 3, 1, 0, 0);
	vkCmdEndRendering(cb);
}