#pragma once

#include "window.h"
#include "scene.h"

typedef struct game game_t;

game_t* game_create(window_t* window);
void game_destroy(game_t* game);

int game_window_event(game_t* game, const window_event_t* event);
void TickGame(game_t* game, float deltaTime);
int game_render(scb_t* scb, game_t* game);
