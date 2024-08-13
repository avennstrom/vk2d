#include "particles.h"
#include "common.h"
#include "vec.h"
#include "rng.h"
#include "../shaders/gpu_types.h"

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>

#define MAX_PARTICLE_COUNT (64 * 1024)
#define PARTICLE_BUFFER_SIZE (MAX_PARTICLE_COUNT * sizeof(gpu_particle_t))

typedef struct particle_effect_state particle_effect_state_t;

typedef struct particle_tick_external_state
{
	const wind_t*	wind;
} particle_tick_external_state_t;

typedef struct footstep_dust_particle
{
	vec2	pos;
	vec2	vel;
	float	lifetime;
	float	age;
} footstep_dust_particle_t;
static void particles_footstep_dust_spawn(particle_effect_state_t* state, particle_spawn_t spawn);
static void particles_footstep_dust_tick(particle_effect_state_t* state, const particle_tick_external_state_t* external);
static void particles_footstep_dust_render(gpu_particle_t* gpuParticles, const particle_effect_state_t* state);

typedef struct ambient_pollen_particle
{
	vec2	pos;
	vec2	vel;
	float	lifetime;
	float	age;
} ambient_pollen_particle_t;
static void particles_ambient_pollen_spawn(particle_effect_state_t* state, particle_spawn_t spawn);
static void particles_ambient_pollen_tick(particle_effect_state_t* state, const particle_tick_external_state_t* external);
static void particles_ambient_pollen_render(gpu_particle_t* gpuParticles, const particle_effect_state_t* state);

typedef void(particles_spawn_function)(particle_effect_state_t* state, particle_spawn_t spawn);
typedef void(particles_tick_function)(particle_effect_state_t* state, const particle_tick_external_state_t* external);
typedef void(particles_render_function)(gpu_particle_t* gpuParticles, const particle_effect_state_t* state);

typedef struct particle_effect_info
{
	uint						maxCount;
	size_t						particleStateSize;
	
	particles_spawn_function*	spawn;
	particles_tick_function*	tick;
	particles_render_function*	render;
} particle_effect_info_t;

static const particle_effect_info_t g_particleEffectInfo[PARTICLE_EFFECT_COUNT] =
{
	[PARTICLE_EFFECT_FOOTSTEP_DUST] = {
		1024,
		sizeof(footstep_dust_particle_t),
		particles_footstep_dust_spawn,
		particles_footstep_dust_tick,
		particles_footstep_dust_render,
	},
	[PARTICLE_EFFECT_AMBIENT_POLLEN] = {
		4 * 1024,
		sizeof(ambient_pollen_particle_t),
		particles_ambient_pollen_spawn,
		particles_ambient_pollen_tick,
		particles_ambient_pollen_render,
	},
};

typedef struct particles_frame
{
	VkBuffer	particleBuffer;
	void*		particleBufferMemory;
	uint		gpuParticleCount;
} particles_frame_t;

typedef struct particle_effect_state
{
	uint	rng;
	uint	count;
	void*	particles;
} particle_effect_state_t;

typedef struct particles
{
	vulkan_t*			vulkan;
	wind_t*				wind;
	particles_frame_t	frames[FRAME_COUNT];
	float				elapsedTime;

	uint				rng;

	particle_effect_state_t	effectState[PARTICLE_EFFECT_COUNT];
} particles_t;

static uint pack_size_and_layer(float size, uint layer)
{
	assert(size >= 0.0f && size <= 1.0f);
	assert(layer <= 0xff);
	return (uint)(size * (float)0xffff) | (layer << 16);
}

particles_t* particles_create(vulkan_t* vulkan, wind_t* wind)
{
	particles_t* particles = calloc(1, sizeof(particles_t));
	if (particles == NULL)
	{
		return NULL;
	}

	particles->vulkan	= vulkan;
	particles->wind		= wind;
	
	for (int i = 0; i < PARTICLE_EFFECT_COUNT; ++i)
	{
		const particle_effect_info_t* info = &g_particleEffectInfo[i];
		particles->effectState[i].particles = calloc(info->maxCount, info->particleStateSize);
		assert(particles->effectState[i].particles != NULL);
	}

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

	const particle_tick_external_state_t external = {
		.wind = particles->wind,
	};
	
	for (int effectIndex = 0; effectIndex < PARTICLE_EFFECT_COUNT; ++effectIndex)
	{
		const particle_effect_info_t* info = &g_particleEffectInfo[effectIndex];
		particle_effect_state_t* state = &particles->effectState[effectIndex];

		(*info->tick)(state, &external);
		
#if 0
		printf("particle count [%d]: %u\n", effectIndex, state->count);
#endif
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
			.sizeAndLayer = pack_size_and_layer(0.05f + sr * 0.05f, 0),
			.color = 0xff000000 | (int)(r * 255.0f) | ((int)((1.0f - r) * 255.0f) << 8),
		};
	}
	gpuParticles += 64;
	frame->gpuParticleCount += 64;
#endif
	
	for (int effectIndex = 0; effectIndex < PARTICLE_EFFECT_COUNT; ++effectIndex)
	{
		const particle_effect_info_t* info = &g_particleEffectInfo[effectIndex];
		particle_effect_state_t* state = &particles->effectState[effectIndex];

		(*info->render)(gpuParticles, state);

		gpuParticles += state->count;
		frame->gpuParticleCount += state->count;
	}
}


void particles_spawn(particles_t* particles, particle_effect_t effect, particle_spawn_t spawn)
{
	const particle_effect_info_t* info = &g_particleEffectInfo[effect];
	particle_effect_state_t* state = &particles->effectState[effect];

	(*info->spawn)(state, spawn);
}

void particles_get_render_info(particles_render_info_t* info, particles_t* particles, uint frameIndex)
{
	particles_frame_t* frame = &particles->frames[frameIndex];

	info->particleCount		= frame->gpuParticleCount;
	info->particleBuffer	= (VkDescriptorBufferInfo){frame->particleBuffer, 0, info->particleCount * sizeof(gpu_particle_t)};
}

static void particles_footstep_dust_spawn(particle_effect_state_t* state, particle_spawn_t spawn)
{
	const uint spawnCount = 5 + (lcg_rand(&state->rng) % 5);
	
	footstep_dust_particle_t* spawnParticles = (footstep_dust_particle_t*)state->particles + state->count;
	for (uint i = 0; i < spawnCount; ++i)
	{
		const float x = lcg_randf(&state->rng) * 2.0f - 1.0f;
		spawnParticles[i] = (footstep_dust_particle_t){
			.lifetime = 1000.0f,
			.pos.x = spawn.pos.x + x * 0.1f,
			.pos.y = spawn.pos.y,
			.vel.x = x * 0.001f,
			.vel.y = lcg_randf_range(&state->rng, 0.0005f, 0.001f),
		};
	}
	
	state->count += spawnCount;
}

static void particles_footstep_dust_tick(particle_effect_state_t* state, const particle_tick_external_state_t* external)
{
	footstep_dust_particle_t* particles = (footstep_dust_particle_t*)state->particles;

	for (uint i = 0; i < state->count;)
	{
		footstep_dust_particle_t* p = &particles[i];
		p->pos = vec2_add(p->pos, vec2_scale(p->vel, DELTA_TIME_MS));
		p->vel = vec2_scale(p->vel, 0.9f);
		p->age += DELTA_TIME_MS;
		
		const bool isDead = p->age >= p->lifetime;
		if (isDead)
		{
			particles[i] = particles[--state->count];
		}
		else
		{
			++i;
		}
	}
}

static void particles_footstep_dust_render(gpu_particle_t* gpuParticles, const particle_effect_state_t* state)
{
	const footstep_dust_particle_t* particles = (footstep_dust_particle_t*)state->particles;

	for (uint i = 0; i < state->count; ++i)
	{
		const footstep_dust_particle_t* p = &particles[i];

		const float tage = p->age / p->lifetime;

		gpuParticles[i] = (gpu_particle_t){
			.center = p->pos,
			.color = 0xff001020,
			.sizeAndLayer = pack_size_and_layer(lerpf(0.2f, 0.0f, tage), 0),
		};
	}
}

static void particles_ambient_pollen_spawn(particle_effect_state_t* state, particle_spawn_t spawn)
{
	const uint spawnCount = 1;
	
	ambient_pollen_particle_t* spawnParticles = (ambient_pollen_particle_t*)state->particles + state->count;
	for (uint i = 0; i < spawnCount; ++i)
	{
		const float x = lcg_randf(&state->rng) * 2.0f - 1.0f;
		spawnParticles[i] = (ambient_pollen_particle_t){
			.lifetime = lcg_randf_range(&state->rng, 10000.0f, 15000.0f),
			.pos.x = spawn.pos.x,
			.pos.y = spawn.pos.y,
			.vel.x = x * 0.0001f,
			.vel.y = lcg_randf_range(&state->rng, 0.00005f, 0.0003f),
		};
	}
	
	state->count += spawnCount;
}

static void particle_sample_wind(vec2* pos, float amount, const particle_tick_external_state_t* external)
{
	*pos = vec2_add(*pos, vec2_scale(wind_sample(external->wind, *pos), amount * DELTA_TIME_MS));
}

static void particles_ambient_pollen_tick(particle_effect_state_t* state, const particle_tick_external_state_t* external)
{
	ambient_pollen_particle_t* particles = (ambient_pollen_particle_t*)state->particles;

	for (uint i = 0; i < state->count;)
	{
		ambient_pollen_particle_t* p = &particles[i];
		particle_sample_wind(&p->pos, 0.004f, external);
		p->pos = vec2_add(p->pos, vec2_scale(p->vel, DELTA_TIME_MS));
		p->pos.x += sinf(p->age * 0.0025f) * DELTA_TIME_MS * 0.0004f;
		p->age += DELTA_TIME_MS;
		
		const bool isDead = p->age >= p->lifetime;
		if (isDead)
		{
			particles[i] = particles[--state->count];
		}
		else
		{
			++i;
		}
	}
}

static void particles_ambient_pollen_render(gpu_particle_t* gpuParticles, const particle_effect_state_t* state)
{
	const ambient_pollen_particle_t* particles = (ambient_pollen_particle_t*)state->particles;

	for (uint i = 0; i < state->count; ++i)
	{
		const ambient_pollen_particle_t* p = &particles[i];

		const float tage = p->age / p->lifetime;

		gpuParticles[i] = (gpu_particle_t){
			.center = p->pos,
			.color = 0xffffffff,
			.sizeAndLayer = pack_size_and_layer(lerpf(0.08f, 0.0f, tage), 1),
			//.size = 0.05f,
		};
	}
}