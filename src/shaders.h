#pragma once

#include "vulkan.h"

enum shader
{
	SHADER_DEBUG_VERT,
	SHADER_DEBUG_FRAG,
	SHADER_SPRITE_VERT,
	SHADER_SPRITE_FRAG,
	SHADER_COMPOSITE_VERT,
	SHADER_COMPOSITE_FRAG,
	SHADER_WORLD_VERT,
	SHADER_WORLD_FRAG,
	SHADER_WORLD_SHADOW_VERT,
	SHADER_WORLD_SHADOW_GEOM,
	SHADER_MODEL_VERT,
	SHADER_MODEL_FRAG,
	SHADER_COUNT,
};

typedef struct shader_library
{
	VkShaderModule modules[SHADER_COUNT];
} shader_library_t;

extern shader_library_t g_shaders;

int InitShaderLibrary(vulkan_t* vulkan);
void DeinitShaderLibrary(vulkan_t* vulkan);
