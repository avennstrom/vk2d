#version 460
#extension GL_GOOGLE_include_directive : require
#include "common.h"

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 1, std140) readonly buffer LightBuffer GPU_LIGHT_BUFFER_BLOCK g_scene;

void main()
{
	const uint lightIndex = gl_InstanceIndex;
	
	vec3 pos = inPosition;
	pos *= vec3(32, 8, 32) * vec3(3.5f, 5.0f, 3.5f);

	gl_PointSize = lightIndex + 0.5f;
	gl_Position = vec4(pos, 1.0) * g_scene.spotLightMatrices[lightIndex];
	gl_Position.y *= -1;
}
