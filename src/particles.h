#pragma once

#include "types.h"
#include "vulkan.h"
#include "staging_memory.h"
#include "render_context.h"

typedef struct particles particles_t;

particles_t* particles_create(vulkan_t* vulkan);
void particles_destroy(particles_t* particles);

void particles_alloc_staging_mem(staging_memory_allocator_t* allocator, particles_t* particles);

void particles_tick(particles_t* particles);
void particles_render(particles_t* particles, const render_context_t* rc);

typedef enum particle_effect
{
	PARTICLE_EFFECT_FOOTSTEP_DUST,
	PARTICLE_EFFECT_COUNT,
} particle_effect_t;

typedef struct particle_spawn
{
	particle_effect_t	effect;
	vec2				pos;
} particle_spawn_t;

void particles_spawn(particles_t* particles, particle_spawn_t spawn);

typedef struct particles_render_info
{
	uint					particleCount;
	VkDescriptorBufferInfo	particleBuffer;
} particles_render_info_t;

void particles_get_render_info(particles_render_info_t* info, particles_t* particles, uint frameIndex);