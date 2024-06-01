#pragma once

#include <stdint.h>

#define countof(Array) (sizeof(Array) / sizeof(Array[0]))

uint32_t alignUp(uint32_t value, uint32_t alignment);
float clampf(float min, float value, float max);