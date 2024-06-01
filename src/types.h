#pragma once

#include <stdint.h>

typedef struct int3 {
	int		x;
	int		y;
	int		z;
} int3;

typedef struct vec2 {
	float	x;
	float	y;
} vec2;

typedef struct vec3 {
	float	x;
	float	y;
	float	z;
} vec3;

typedef struct vec4 {
	float	x;
	float	y;
	float	z;
	float	w;
} vec4;

typedef struct mat4 {
	vec4	r0;
	vec4	r1;
	vec4	r2;
	vec4	r3;
} mat4;

typedef struct ushort2 {
	uint16_t x;
	uint16_t y;
} ushort2;

typedef struct ushort3 {
	uint16_t x;
	uint16_t y;
	uint16_t z;
} ushort3;

typedef struct short3 {
	int16_t x;
	int16_t y;
	int16_t z;
} short3;

typedef struct uint2 {
	uint32_t x;
	uint32_t y;
} uint2;

typedef struct uint3 {
	uint32_t x;
	uint32_t y;
	uint32_t z;
} uint3;

typedef uint8_t byte;
typedef uint16_t ushort;
typedef uint32_t uint;
typedef vec3 float3;