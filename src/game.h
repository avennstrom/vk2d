#pragma once

#include "window.h"
#include "scene.h"

typedef struct game game_t;

typedef struct game_viewport
{
	uint	width;
	uint	height;
} game_viewport_t;

game_t* game_create(window_t* window, const model_loader_t* modelLoader);
void game_destroy(game_t* game);

int game_window_event(game_t* game, const window_event_t* event);
void game_tick(game_t* game, float deltaTime, const game_viewport_t* viewport);
int game_render(scb_t* scb, game_t* game);
