#include "rng.h"

uint32_t lcg_rand(uint32_t* rng) {
	*rng = (LCG_MULTIPLIER * *rng + LCG_INCREMENT) % LCG_MODULUS;
	return *rng;
}

float lcg_randf(uint32_t* rng) {
	return lcg_rand(rng) / (float)LCG_MODULUS;
}