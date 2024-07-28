#include "intersection.h"
#include "vec.h"

bool intersect_ray_plane(float* hitDistance, vec3 origin, vec3 direction, vec4 plane)
{
	const float denominator = vec3_dot(vec4_xyz(plane), direction);
	if (denominator == 0.0f)
	{
		return false;
	}

	const float t = (plane.w - vec3_dot(vec4_xyz(plane), origin)) / denominator;
	if (t < 0.0f)
	{
		return false;
	}
	
	*hitDistance = t;
	return true;
}