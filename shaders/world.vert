#version 450
#extension GL_GOOGLE_include_directive : require
#include "gpu_types.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inNormal;
layout(location = 2) in uint inFlags0;
layout(location = 3) in uint inFlags1;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPos;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec2 fragUv;
layout(location = 4) out flat uint outFlags0;
layout(location = 5) out flat uint outFlags1;

layout(set = 0, binding = 0) uniform FrameUniformBlock {
    gpu_frame_uniforms_t g_frame;
};

void main()
{
	const uint packedNormal = inNormal & 0x3f;

	vec3 normal = vec3(0, 0, 0);

	if ((packedNormal & 1) != 0) {
		normal.z = -1;
	}
	else if ((packedNormal & 4) != 0) {
		normal.z = 1;
	}
	if ((packedNormal & 2) != 0) {
		normal.x = 1;
	}
	else if ((packedNormal & 8) != 0) {
		normal.x = -1;
	}
	if ((packedNormal & 16) != 0) {
		normal.y = 1;
	}
	else if ((packedNormal & 32) != 0) {
		normal.y = -1;
	}
	normal = normalize(normal);

	if ((inFlags0 & (1 << 31)) != 0) {
		const uint px = (inFlags0 >> 0) & 0x3;
		const uint py = (inFlags0 >> 2) & 0x3;
		fragUv.x = (px + ((inFlags0 >> 4) & 1)) / 4.0f;
    	fragUv.y = (py + ((inFlags0 >> 5) & 1)) / 4.0f;
	}


	vec3 color = 0.5.rrr;

	vec3 pos = inPosition;
	pos *= vec3(32, 8, 32) * vec3(3.5f, 5.0f, 3.5f);
	
	fragColor = color;
	fragPos = pos;
	fragNormal = normal;
	outFlags0 = inFlags0;
	outFlags1 = inFlags1;

	gl_Position = vec4(pos, 1.0) * g_frame.matViewProj;
	gl_Position.y *= -1;
}