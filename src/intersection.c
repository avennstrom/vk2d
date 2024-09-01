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

float sign(vec2 p1, vec2 p2, vec2 p3)
{
	return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool intersect_point_triangle_2d(vec2 p, vec2 v0, vec2 v1, vec2 v2)
{
	float d1, d2, d3;
	bool has_neg, has_pos;

	d1 = sign(p, v0, v1);
	d2 = sign(p, v1, v2);
	d3 = sign(p, v2, v0);

	has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

	return !(has_neg && has_pos);
}