#include "vec.h"

#include <math.h>

vec2 vec2_add(vec2 a, vec2 b)
{
	return (vec2){ a.x + b.x, a.y + b.y };
}

vec2 vec2_sub(vec2 a, vec2 b)
{
	return (vec2){ a.x - b.x, a.y - b.y };
}

float vec2_length(vec2 v)
{
	return sqrtf(vec2_dot(v, v));
}

vec2 vec2_normalize(vec2 v)
{
	const float rlen = 1.0f / vec2_length(v);
	return vec2_scale(v, rlen);
}

float vec2_dot(vec2 a, vec2 b)
{
	return (a.x * b.x) + (a.y * b.y);
}

vec2 vec2_scale(vec2 v, float s)
{
	return (vec2){ v.x * s, v.y * s };
}

vec2 vec2_lerp(vec2 a, vec2 b, float t)
{
	return (vec2){
		a.x + t * (b.x - a.x),
		a.y + t * (b.y - a.y),
	};
}

vec3 vec3_add(vec3 a, vec3 b)
{
	return (vec3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

vec3 vec3_sub(vec3 a, vec3 b)
{
	return (vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}

float vec3_length(vec3 v)
{
	return sqrtf(vec3_dot(v, v));
}

vec3 vec3_normalize(vec3 v)
{
	const float rlen = 1.0f / vec3_length(v);
	return vec3_scale(v, rlen);
}

float vec3_dot(vec3 a, vec3 b)
{
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

vec3 vec3_scale(vec3 v, float s)
{
	return (vec3){ v.x * s, v.y * s, v.z * s };
}

vec3 vec4_xyz(vec4 v)
{
	return (vec3){ v.x, v.y, v.z };
}

vec2 vec3_xy(vec3 v)
{
	return (vec2){ v.x, v.y };
}

vec2 vec3_xz(vec3 v)
{
	return (vec2){ v.x, v.z };
}