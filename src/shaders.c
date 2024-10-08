#include "shaders.h"
#include "util.h"

#include <stdio.h>
#include <assert.h>

#define SHADER_BLOB(Name) \
	extern const char _binary_obj_##Name##_spv_start[]; \
	extern const char _binary_obj_##Name##_spv_end[];

#define SHADER_ARG_HELPER(Name) \
	_binary_obj_##Name##_spv_start, _binary_obj_##Name##_spv_end, #Name

SHADER_BLOB(debug_vs);
SHADER_BLOB(debug_fs);
SHADER_BLOB(composite_vs);
SHADER_BLOB(composite_fs);
SHADER_BLOB(world_vs);
SHADER_BLOB(world_fs);
SHADER_BLOB(model_vs);
SHADER_BLOB(model_fs);
SHADER_BLOB(particle_vs);
SHADER_BLOB(particle_fs);

shader_library_t g_shaders = {};

static VkShaderModule createShaderModule(vulkan_t* vulkan, const char* spirvStart, const char* spirvEnd, const char* debugName)
{
	const size_t len = (size_t)(ptrdiff_t)(spirvEnd - spirvStart);
	assert((len % 4) == 0);
	
	const VkShaderModuleCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = (uint32_t*)spirvStart,
		.codeSize = len,
	};

	VkShaderModule module;
	VkResult r = vkCreateShaderModule(vulkan->device, &createInfo, NULL, &module);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkCreateShaderModule failed\n");
		return VK_NULL_HANDLE;
	}

	SetShaderModuleName(vulkan, module, debugName);
	return module;
}

int InitShaderLibrary(vulkan_t* vulkan)
{
	g_shaders.modules[SHADER_DEBUG_VERT] = createShaderModule(vulkan, SHADER_ARG_HELPER(debug_vs));
	g_shaders.modules[SHADER_DEBUG_FRAG] = createShaderModule(vulkan, SHADER_ARG_HELPER(debug_fs));
	g_shaders.modules[SHADER_COMPOSITE_VERT] = createShaderModule(vulkan, SHADER_ARG_HELPER(composite_vs));
	g_shaders.modules[SHADER_COMPOSITE_FRAG] = createShaderModule(vulkan, SHADER_ARG_HELPER(composite_fs));
	g_shaders.modules[SHADER_WORLD_VERT] = createShaderModule(vulkan, SHADER_ARG_HELPER(world_vs));
	g_shaders.modules[SHADER_WORLD_FRAG] = createShaderModule(vulkan, SHADER_ARG_HELPER(world_fs));
	g_shaders.modules[SHADER_MODEL_VERT] = createShaderModule(vulkan, SHADER_ARG_HELPER(model_vs));
	g_shaders.modules[SHADER_MODEL_FRAG] = createShaderModule(vulkan, SHADER_ARG_HELPER(model_fs));
	g_shaders.modules[SHADER_PARTICLE_VERT] = createShaderModule(vulkan, SHADER_ARG_HELPER(particle_vs));
	g_shaders.modules[SHADER_PARTICLE_FRAG] = createShaderModule(vulkan, SHADER_ARG_HELPER(particle_fs));
	return 0;
}

void DeinitShaderLibrary(vulkan_t* vulkan)
{
	for (int i = 0; i < SHADER_COUNT; ++i)
	{
		vkDestroyShaderModule(vulkan->device, g_shaders.modules[i], NULL);
	}
}