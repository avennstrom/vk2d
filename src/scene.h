#pragma once

#include "mat.h"
#include "vulkan.h"
#include "worldgen.h"
#include "painter.h"
#include "render_targets.h"
#include "descriptors.h"
#include "staging_memory.h"
#include "debug_renderer.h"

#include <stddef.h>
#include <stdint.h>

typedef struct scene scene_t;
typedef struct scb scb_t; // scene command buffer

typedef struct model_handle {
  uint32_t index;
} model_handle_t;

scene_t *new_scene(vulkan_t *vulkan);
void delete_scene(scene_t *scene);
int AllocateSceneStagingMemory(staging_memory_allocator_t* allocator, scene_t *scene);

scb_t *scene_begin(scene_t *scene);

typedef struct scb_camera {
  vec3 pos;
  float pitch;
  float yaw;
} scb_camera_t;

typedef struct scb_draw_model {
  model_handle_t model;
  mat4 transform;
} scb_draw_model_t;

typedef struct scb_point_light {
  vec3 position;
  float radius;
  vec3 color;
} scb_point_light_t;

typedef struct scb_spot_light {
  mat4 transform;
  vec3 color;
  float range;
  float radius;
} scb_spot_light_t;

scb_camera_t *scb_set_camera(scb_t *scb);

scb_draw_model_t *scb_draw_models(scb_t *scb, size_t count);

scb_point_light_t *scb_add_point_lights(
  scb_t *scb,
  size_t count);
  
scb_spot_light_t *scb_add_spot_lights(
  scb_t *scb,
  size_t count);

void DrawScene(
  VkCommandBuffer cb,
  scene_t* scene,
  vulkan_t* vulkan,
  render_targets_t* rt,
  scb_t* scb,
  descriptor_allocator_t* dsalloc,
  staging_memory_context_t* stagingMemory,
  debug_renderer_t* debugRenderer,
  worldgen_t* worldgen,
  painter_t* painter,
  size_t frameIndex);