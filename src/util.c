#include "util.h"

uint32_t max(uint32_t a, uint32_t b)
{
	if (b > a) return b;
	return a;
}

uint32_t alignUp(uint32_t value, uint32_t alignment)
{
	return (value + alignment - 1) / alignment * alignment;
}

float clampf(float min, float value, float max)
{
	if (value < min) value = min;
	if (value > max) value = max;
	return value;
}