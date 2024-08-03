#pragma once

#define MAX_DRAWS (1024)
#define MAX_POINT_LIGHTS (64)

#ifdef __STDC__
typedef struct gpu_draw_t gpu_draw_t;
typedef struct gpu_point_light_t gpu_point_light_t;
typedef struct gpu_frame_uniforms_t gpu_frame_uniforms_t;
#endif

#ifndef __STDC__
typedef float2 vec2;
typedef float3 vec3;
typedef float4 vec4;
typedef float4x4 mat4;
#endif

struct gpu_draw_t
{
	uint	indexCount;
	uint	instanceCount;
	uint	firstIndex;
	int		vertexOffset;

	uint	firstInstance;
	uint	vertexPositionOffset;
	uint	vertexNormalOffset;
	uint	vertexColorOffset;

	mat4	transform;
};

struct gpu_point_light_t
{
	vec3	pos;
	float	radius;
	vec3	color;
	float	_pad0;
};

struct gpu_frame_uniforms_t
{
	mat4	matViewProj;
	uint	drawCount;
	uint	pointLightCount;
	uint	spotLightCount;
	uint	_pad0;
};

#ifdef __STDC__
_Static_assert(sizeof(gpu_draw_t) == 96, "");
_Static_assert(sizeof(gpu_point_light_t) == 32, "");
_Static_assert(sizeof(gpu_frame_uniforms_t) == 80, "");
#endif