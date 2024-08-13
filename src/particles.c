#include "particles.h"
#include "common.h"
#include "vec.h"
#include "../shaders/gpu_types.h"

#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

#define MAX_PARTICLE_COUNT (64 * 1024)
#define PARTICLE_BUFFER_SIZE (MAX_PARTICLE_COUNT * sizeof(gpu_particle_t))

typedef struct particle_effect_info
{
	uint	maxCount;
} particle_effect_info_t;

static const particle_effect_info_t g_particleEffectInfo[PARTICLE_EFFECT_COUNT] =
{
	[PARTICLE_EFFECT_FOOTSTEP_DUST] = { .maxCount = 1024 },
};

typedef struct footstep_dust_particle
{
	vec2	pos;
	vec2	vel;
	float	lifetime;
	float	age;
} footstep_dust_particle_t;

typedef struct particles_frame
{
	VkBuffer	particleBuffer;
	void*		particleBufferMemory;
	uint		gpuParticleCount;
} particles_frame_t;

typedef struct particles
{
	vulkan_t*			vulkan;
	particles_frame_t	frames[FRAME_COUNT];
	float				elapsedTime;

	uint				rng;

	uint						footstepDustParticleCount;
	footstep_dust_particle_t*	footstepDustParticles;
} particles_t;

#define LCG_MULTIPLIER	1664525u
#define LCG_INCREMENT	1013904223u

static uint lcg_rand(uint* rng)
{
	*rng = LCG_MULTIPLIER * (*rng) + LCG_INCREMENT;
	return *rng;
}

static float lcg_randf(uint* rng)
{
	return lcg_rand(rng) / (float)0xffffffffu;
}

static float lcg_randf_range(uint* rng, float a, float b)
{
	const float t = lcg_randf(rng);
	return lerpf(a, b, t);
}

particles_t* particles_create(vulkan_t* vulkan)
{
	particles_t* particles = calloc(1, sizeof(particles_t));
	if (particles == NULL)
	{
		return NULL;
	}

	particles->vulkan	= vulkan;

	particles->footstepDustParticles = calloc(g_particleEffectInfo[PARTICLE_EFFECT_FOOTSTEP_DUST].maxCount, sizeof(particles_t));

	return particles;
}

void particles_destroy(particles_t* particles)
{
	
}

void particles_alloc_staging_mem(staging_memory_allocator_t* allocator, particles_t* particles)
{
	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		particles_frame_t* frame = &particles->frames[i];

		PushStagingBufferAllocation(
			allocator,
			&frame->particleBuffer,
			&frame->particleBufferMemory,
			PARTICLE_BUFFER_SIZE,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			"Particles");
	}
}

void particles_tick(particles_t* particles)
{
	particles->elapsedTime += DELTA_TIME_MS;

	for (uint i = 0; i < particles->footstepDustParticleCount;)
	{
		footstep_dust_particle_t* dustParticle = &particles->footstepDustParticles[i];
		dustParticle->pos = vec2_add(dustParticle->pos, vec2_scale(dustParticle->vel, DELTA_TIME_MS));
		dustParticle->vel = vec2_scale(dustParticle->vel, 0.9f);
		dustParticle->age += DELTA_TIME_MS;
		
		const bool isDead = dustParticle->age >= dustParticle->lifetime;
		if (isDead)
		{
			--particles->footstepDustParticleCount;
			particles->footstepDustParticles[i] = particles->footstepDustParticles[particles->footstepDustParticleCount];
		}
		else
		{
			++i;
		}
	}
}

void particles_render(particles_t* particles, const render_context_t* rc)
{
	particles_frame_t* frame = &particles->frames[rc->frameIndex];

	frame->gpuParticleCount = 0;
	
	gpu_particle_t* gpuParticles = (gpu_particle_t*)frame->particleBufferMemory;

#if 1
	srand(42);
	for (int i = 0; i < 64; ++i)
	{
		const float x = rand() / (float)RAND_MAX;
		const float y = rand() / (float)RAND_MAX;
		const float t = (rand() / (float)RAND_MAX) / 100.0f + 0.001f;
		const float s = (rand() / (float)RAND_MAX) * 3.14f;
		const float sr = sinf(t + s);
		const float r = (sr * 0.5f + 0.5f);
		gpuParticles[i] = (gpu_particle_t){
			.center.x = sinf(particles->elapsedTime * t) * x,
			.center.y = cosf(particles->elapsedTime * t) * y,
			.size = 0.05f + sr * 0.05f,
			.color = 0xff000000 | (int)(r * 255.0f) | ((int)((1.0f - r) * 255.0f) << 8),
		};
	}
	gpuParticles += 64;
	frame->gpuParticleCount += 64;
#endif
	
	for (uint i = 0; i < particles->footstepDustParticleCount; ++i)
	{
		const footstep_dust_particle_t* dustParticle = &particles->footstepDustParticles[i];

		const float tage = dustParticle->age / dustParticle->lifetime;

		gpuParticles[i] = (gpu_particle_t){
			.center = dustParticle->pos,
			.color = 0xff001020,
			.size = lerpf(0.3f, 0.0f, tage),
		};
	}

	gpuParticles += particles->footstepDustParticleCount;
	frame->gpuParticleCount += particles->footstepDustParticleCount;
}

static void particles_spawn_footstep_dust(particles_t* particles, particle_spawn_t spawn)
{
	const uint spawnCount = 5 + (lcg_rand(&particles->rng) % 5);
	
	footstep_dust_particle_t* spawnParticles = particles->footstepDustParticles + particles->footstepDustParticleCount;
	for (uint i = 0; i < spawnCount; ++i)
	{
		const float x = lcg_randf(&particles->rng) * 2.0f - 1.0f;
		spawnParticles[i] = (footstep_dust_particle_t){
			.lifetime = 1000.0f,
			.pos.x = spawn.pos.x + x * 0.1f,
			.pos.y = spawn.pos.y,
			.vel.x = x * 0.001f,
			.vel.y = lcg_randf_range(&particles->rng, 0.0005f, 0.004f),
		};
	}
	
	particles->footstepDustParticleCount += spawnCount;
}

void particles_spawn(particles_t* particles, particle_spawn_t spawn)
{
	switch (spawn.effect)
	{
		case PARTICLE_EFFECT_FOOTSTEP_DUST:	particles_spawn_footstep_dust(particles, spawn); break;
	}
}

void particles_get_render_info(particles_render_info_t* info, particles_t* particles, uint frameIndex)
{
	particles_frame_t* frame = &particles->frames[frameIndex];

	info->particleCount		= frame->gpuParticleCount;
	info->particleBuffer	= (VkDescriptorBufferInfo){frame->particleBuffer, 0, info->particleCount * sizeof(gpu_particle_t)};
}
