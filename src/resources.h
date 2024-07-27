
#include <stdint.h>
#include <types.h>

#define MODEL_TANK	0

typedef struct model_tank
{
	uint32_t node_body;
	uint32_t node_turret;
	uint32_t node_pipe;
} model_tank_t;

typedef struct models
{
	model_tank_t	tank;
} models_t;

typedef struct game_model_hierarchy
{
	uint32_t	parentIndex[1024];
	mat4		localTransform[1024];
} game_model_hierarchy_t;

typedef struct game_resource
{
	models_t				models;
	game_model_hierarchy_t	hierarchy;
} game_resource_t;

//void resource_manifest_load(resource_manifest_t* manifest);

//void models_load(model_tank_t* model, model_loader_t* modelLoader);
