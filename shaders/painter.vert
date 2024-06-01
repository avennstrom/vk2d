#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.h"

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inSize;

layout(location = 0) out vec3 fragColor;

void main()
{
	fragColor = inColor;
	gl_Position = vec4(inPosition, 0.0, 1.0);
	gl_PointSize = inSize;
}