#pragma once

#include <stdint.h>

#define LCG_MULTIPLIER 1664525
#define LCG_INCREMENT 1013904223
#define LCG_MODULUS 4294967296 // 2^32

uint32_t lcg_rand(uint32_t* rng);
float lcg_randf(uint32_t* rng);
