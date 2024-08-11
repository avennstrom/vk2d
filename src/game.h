#pragma once

#include "window.h"
#include "scene.h"
#include "world.h"
#include "wind.h"

typedef struct game game_t;

game_t* game_create(window_t* window, const model_loader_t* modelLoader, world_t* world, wind_t* wind);
void game_destroy(game_t* game);

int game_window_event(game_t* game, const window_event_t* event);
void game_tick(game_t* game, float deltaTime, uint2 resolution);
int game_render(scb_t* scb, game_t* game);
