#pragma once

#include "types.h"

#include <stdbool.h>

bool intersect_ray_plane(float* hitDistance, vec3 origin, vec3 direction, vec4 plane);

bool intersect_point_triangle_2d(vec2 p, vec2 v0, vec2 v1, vec2 v2);