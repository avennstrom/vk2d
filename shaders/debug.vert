#version 450
#extension GL_GOOGLE_include_directive : require
#include "gpu_types.h"

layout(set = 0, binding = 0) uniform FrameUniformBlock {
    gpu_frame_uniforms_t g_frame;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main()
{
	gl_PointSize = 8.0;
	fragColor = inColor;
	gl_Position = vec4(inPosition, 1.0) * g_frame.matViewProj;
	gl_Position.y *= -1;
}