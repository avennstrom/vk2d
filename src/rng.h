#pragma once

#include "types.h"

uint lcg_rand(uint* rng);
float lcg_randf(uint* rng);
float lcg_randf_range(uint* rng, float a, float b);
