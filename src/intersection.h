#pragma once

#include "types.h"

#include <stdbool.h>

bool intersect_ray_plane(float* hitDistance, vec3 origin, vec3 direction, vec4 plane);