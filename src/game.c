#include "game.h"
#include "mat.h"
#include "scene.h"
#include "util.h"
#include "rng.h"
#include "intersection.h"
#include "vec.h"

#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <memory.h>
#include <stdbool.h>

#define PLAYER_SPEED		0.01f
#define PLAYER_SPEED_BOOST	4.0f

typedef struct model_tank
{
	uint32_t node_body;
	uint32_t node_turret;
	uint32_t node_pipe;
} model_tank_t;

static const model_tank_t g_model_tank = {
	.node_body = 0,
	.node_turret = 1,
	.node_pipe = 2,
};

typedef struct player {
	vec2	pos;
} player_t;

enum {
	GameState_LoadScene = 0,
	GameState_Play,
};

typedef struct game {
	window_t*				window;
	const model_loader_t*	modelLoader;
	int						state;
	//bool					lockMouse;
	float					aspectRatio;
	int						mouseX;
	int						mouseY;

	float					t;
	player_t				player;
	float					viewHeight;

	bool					keystate[256];
	bool					buttonstate[2];
} game_t;

game_t* game_create(window_t* window, const model_loader_t* modelLoader)
{
	game_t* game = calloc(1, sizeof(game_t));
	if (game == NULL) {
		return NULL;
	}

	game->window		= window;
	game->modelLoader	= modelLoader;
	game->viewHeight	= 10.0f;

	return game;
}

void game_destroy(game_t* game)
{
	free(game);
}

int game_window_event(game_t* game, const window_event_t* event)
{
	switch (event->type) {
		case WINDOW_EVENT_KEY_DOWN:
			game->keystate[event->data.key.code] = true;
			break;
		case WINDOW_EVENT_KEY_UP:
			game->keystate[event->data.key.code] = false;
			break;
		case WINDOW_EVENT_BUTTON_DOWN:
			// if (event->data.button.button == BUTTON_RIGHT) {
			// 	game->lockMouse = !game->lockMouse;
			// 	window_lock_mouse(game->window, game->lockMouse);
			// }
			game->buttonstate[event->data.button.button] = true;
			break;
		case WINDOW_EVENT_BUTTON_UP:
			game->buttonstate[event->data.button.button] = false;
			break;
		case WINDOW_EVENT_MOUSE_MOVE:
			game->mouseX = event->data.mouse.x;
			game->mouseY = event->data.mouse.y;
			break;
		case WINDOW_EVENT_MOUSE_SCROLL:
			if (event->data.scroll.delta > 0)
			{
				game->viewHeight *= 0.7f;
			}
			if (event->data.scroll.delta < 0)
			{
				game->viewHeight *= 1.3f;
			}
			break;
	}

	return 0;
}

static void TickPlayerMovement(game_t* game, float deltaTime)
{
	// move
	float moveX = 0.0f;
	float moveY = 0.0f;
	
	if (game->keystate[KEY_W]) {
		moveY += 1.0f;
	}
	if (game->keystate[KEY_S]) {
		moveY -= 1.0f;
	}
	if (game->keystate[KEY_A]) {
		moveX -= 1.0f;
	}
	if (game->keystate[KEY_D]) {
		moveX += 1.0f;
	}

	const float moveLen = sqrtf(moveX * moveX + moveY * moveY);
	if (moveLen != 0.0f) {
		moveX /= moveLen;
		moveY /= moveLen;
	}

	moveX *= deltaTime;
	moveY *= deltaTime;

	float speed = PLAYER_SPEED;
	if (game->keystate[KEY_SHIFT]) {
		speed *= PLAYER_SPEED_BOOST;
	}

	game->player.pos.x += moveX * speed;
	game->player.pos.y += moveY * speed;
}

static void calculate_camera(scb_camera_t* camera, const player_t* player, float aspectRatio, float viewHeight)
{
	mat4 m = mat_identity();
	m = mat_translate(m, (vec3){ player->pos.x, player->pos.y, 0.0f });

	const vec2 viewSize = {viewHeight * aspectRatio, viewHeight};

	const mat4 viewMatrix = mat_invert(m);
	const mat4 projectionMatrix = mat_orthographic(viewSize, 0.0f, 1.0f);
	const mat4 viewProjectionMatrix = mat_mul(viewMatrix, projectionMatrix);

	// :TODO: transpose these in the scene instead
	camera->transform = mat_transpose(m);
	camera->viewMatrix = mat_transpose(viewMatrix);
	camera->projectionMatrix = mat_transpose(projectionMatrix);
	camera->viewProjectionMatrix = mat_transpose(viewProjectionMatrix);
}

void game_tick(game_t* game, float deltaTime, const game_viewport_t* viewport)
{
	int r;

	game->aspectRatio = viewport->width / (float)viewport->height;

	switch (game->state) {
		case GameState_LoadScene:
		{
			++game->state;
			break;
		}

		case GameState_Play:
		{
			TickPlayerMovement(game, deltaTime);
		}
	}

	game->t += deltaTime;

	return;
}

int game_render(scb_t* scb, game_t* game)
{
	scb_point_light_t* point_light;

	calculate_camera(scb_set_camera(scb), &game->player, game->aspectRatio, game->viewHeight);

	return 0;
}
