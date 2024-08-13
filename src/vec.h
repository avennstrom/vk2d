#pragma once

#include "types.h"

float lerpf(float a, float b, float t);

vec2 vec2_add(vec2 a, vec2 b);
vec2 vec2_sub(vec2 a, vec2 b);
vec2 vec2_mul(vec2 a, vec2 b);
float vec2_length(vec2 v);
float vec2_distance(vec2 a, vec2 b);
vec2 vec2_normalize(vec2 v);
float vec2_dot(vec2 a, vec2 b);
vec2 vec2_scale(vec2 v, float s);
vec2 vec2_lerp(vec2 a, vec2 b, float t);

vec3 vec3_add(vec3 a, vec3 b);
vec3 vec3_sub(vec3 a, vec3 b);
float vec3_length(vec3 v);
vec3 vec3_normalize(vec3 v);
float vec3_dot(vec3 a, vec3 b);
vec3 vec3_scale(vec3 v, float s);

vec2 vec3_xy(vec3 v);
vec2 vec3_xz(vec3 v);
vec3 vec4_xyz(vec4 v);