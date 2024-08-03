#pragma once

#include "mat.h"
#include "vulkan.h"
#include "render_context.h"
#include "render_targets.h"
#include "descriptors.h"
#include "staging_memory.h"
#include "debug_renderer.h"
#include "model_loader.h"

#include <stddef.h>
#include <stdint.h>

typedef struct scene scene_t;
typedef struct scb scb_t; // scene command buffer

scene_t*	scene_create(vulkan_t *vulkan);
void		scene_destroy(scene_t *scene);
int			scene_alloc_staging_mem(staging_memory_allocator_t* allocator, scene_t *scene);
scb_t*		scene_begin(scene_t *scene);
void		scene_draw(VkCommandBuffer cb, scene_t* scene, scb_t* scb, const render_context_t* rc, debug_renderer_t* debugRenderer);

typedef struct scb_camera {
	mat4 transform;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
} scb_camera_t;

typedef struct scb_draw_model {
	model_handle_t model;
	mat4 transform[16]; // todo
} scb_draw_model_t;

typedef struct scb_point_light {
	vec3 position;
	float radius;
	vec3 color;
} scb_point_light_t;

scb_camera_t* scb_set_camera(
	scb_t* scb);

scb_draw_model_t* scb_draw_models(
	scb_t* scb,
	size_t count);

scb_point_light_t* scb_add_point_lights(
	scb_t* scb,
	size_t count);

