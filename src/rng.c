#include "rng.h"
#include "vec.h"

#define LCG_MULTIPLIER	1664525u
#define LCG_INCREMENT	1013904223u

uint lcg_rand(uint* rng)
{
	*rng = LCG_MULTIPLIER * (*rng) + LCG_INCREMENT;
	return *rng;
}

float lcg_randf(uint* rng)
{
	return lcg_rand(rng) / (float)0xffffffffu;
}

float lcg_randf_range(uint* rng, float a, float b)
{
	const float t = lcg_randf(rng);
	return lerpf(a, b, t);
}