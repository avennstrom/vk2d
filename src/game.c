#include "game.h"
#include "mat.h"
#include "scene.h"
#include "util.h"
#include "rng.h"

#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <memory.h>
#include <stdbool.h>

#define PLAYER_HEIGHT		1.75f
#define PLAYER_SPEED		0.01f
#define PLAYER_SENSITIVITY	0.0015f

#define MAX_ENEMIES			4

typedef struct player {
	byte	flashlight : 1;
	vec3	pos;
	float	yaw;
	float	pitch;
} player_t;

enum {
	GameState_LoadScene = 0,
	GameState_Play,
};

typedef struct game {
	window_t*				window;
	const model_loader_t*	modelLoader;
	int						state;
	bool					lockMouse;

	float					t;
	player_t				player;

	bool					keystate[256];
	bool					buttonstate[2];
} game_t;

static vec3 GetPlayerHead(const player_t* player)
{
	vec3 head = player->pos;
	head.y += PLAYER_HEIGHT;
	return head;
}

static vec3 GetPlayerLookNormal(const player_t* player)
{
	const float p = cosf(player->pitch);

	float3 lookDir;
	lookDir.x = sinf(player->yaw) * p;
	lookDir.y = -sinf(player->pitch);
	lookDir.z = -cosf(player->yaw) * p;

	return lookDir;
}

game_t* game_create(window_t* window, const model_loader_t* modelLoader)
{
	game_t* game = calloc(1, sizeof(game_t));
	if (game == NULL) {
		return NULL;
	}

	game->window		= window;
	game->modelLoader	= modelLoader;

	//game->player.flashlight = true;

	game->player.pos.z = 4.0f;
	game->player.pitch = 0.3f;

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
			if (event->data.key.code == KEY_F) {
				game->player.flashlight = !game->player.flashlight;
			}
			break;
		case WINDOW_EVENT_KEY_UP:
			game->keystate[event->data.key.code] = false;
			break;
		case WINDOW_EVENT_BUTTON_DOWN:
			if (event->data.button.button == BUTTON_RIGHT) {
				game->lockMouse = !game->lockMouse;
				window_lock_mouse(game->window, game->lockMouse);
			}
			game->buttonstate[event->data.button.button] = true;
			break;
		case WINDOW_EVENT_BUTTON_UP:
			game->buttonstate[event->data.button.button] = false;
			break;
		case WINDOW_EVENT_MOUSE_MOVE:
			if (game->buttonstate[BUTTON_LEFT] || game->lockMouse) {
				game->player.yaw += event->data.mouse.dx * PLAYER_SENSITIVITY;
				game->player.pitch += event->data.mouse.dy * PLAYER_SENSITIVITY;
			}
			break;
	}

	return 0;
}

static void TickPlayerMovement(game_t* game, float deltaTime)
{
#if 0
	// look	
	float lookX = 0.0f;
	float lookY = 0.0f;

	if (game->keystate[KEY_LEFT]) {
		lookX -= PLAYER_SENSITIVITY;
	}
	if (game->keystate[KEY_RIGHT]) {
		lookX += PLAYER_SENSITIVITY;
	}
	if (game->keystate[KEY_UP]) {
		lookY -= PLAYER_SENSITIVITY;
	}
	if (game->keystate[KEY_DOWN]) {
		lookY += PLAYER_SENSITIVITY;
	}
	
	game->player.yaw += lookX;
	game->player.pitch += lookY;
	game->player.pitch = clampf(-1.0f, game->player.pitch, 1.0f);
#endif

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

	const float s = sinf(game->player.yaw);
	const float c = cosf(game->player.yaw);

	game->player.pos.x += (moveX * PLAYER_SPEED * c) + (moveY * PLAYER_SPEED * s);
	game->player.pos.z += (moveX * PLAYER_SPEED * s) + (moveY * PLAYER_SPEED * -c);

	if (game->keystate[KEY_SPACE]) {
		game->player.pos.y += PLAYER_SPEED * deltaTime;
	}
	if (game->keystate[KEY_CONTROL]) {
		game->player.pos.y -= PLAYER_SPEED * deltaTime;
	}
}

void TickGame(game_t* game, float deltaTime)
{
	int r;

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
	scb_spot_light_t* spot_light;
	
	*scb_set_camera(scb) = (scb_camera_t){
		.pos	= GetPlayerHead(&game->player),
		.yaw	= game->player.yaw,
		.pitch	= game->player.pitch,
	};
	
	if (game->player.flashlight) {
		float3 head = GetPlayerHead(&game->player);
		//head.x += 2.0f;

		mat4 m = mat_identity();
		m = mat_translate(m, head);
		m = mat_rotate_y(m, game->player.yaw);
		m = mat_rotate_x(m, game->player.pitch);

		spot_light = scb_add_spot_lights(scb, 1);
		spot_light[0] = (scb_spot_light_t){
			.transform	= m,
			.range		= 20.0f,
			.radius		= 90.0f,
			.color		= {0.5f, 0.5f, 0.5f},
		};
	}

	{
		const model_handle_t tankModel = {0};

		mat4 m[16];
		for (int i = 0; i < countof(m); ++i)
		{
			m[i] = mat_identity();
		}

		// Tank
		m[0] = mat_translate(m[0], (vec3){sin(0.001f * game->t) * 2.0f, 0.0f, 0.0f});
		// Turret
		m[1] = mat_rotate_y(m[1], 0.002f * game->t * 0.4f);
		// Pipe
		m[2] = mat_rotate_z(m[2], sin(0.01f * game->t) * 0.2f);

		// m = mat_translate(m, (vec3){2.0f, 0.0f, 0.0f});
		// m = mat_rotate_x(m, 0.001f * game->t);
		// m = mat_rotate_z(m, 0.001f * game->t * 0.6f);
		// m = mat_rotate_y(m, 0.001f * game->t * 0.4f);

		model_hierarchy_t hierarchy;
		model_loader_get_model_hierarchy(&hierarchy, game->modelLoader, tankModel);
		
		mat4 transforms[16];
		model_hierarchy_resolve(transforms, m, &hierarchy);

		scb_draw_model_t* models = scb_draw_models(scb, 1);
		models[0] = (scb_draw_model_t){
			//.model = {(int)(game->t * 0.001f) % 2},
			.model = tankModel,
			.transform[0] = transforms[0],
			.transform[1] = transforms[1],
			.transform[2] = transforms[2],
			.transform[3] = transforms[3],
			.transform[4] = transforms[4],
		};
	}

#if 0
	{
		mat4 m = mat_identity();
		m = mat_translate(m, game->playerSpawn);

		spot_light = scb_add_spot_lights(scb, 1);
		spot_light[0] = (scb_spot_light_t){
			.transform	= m,
			.range		= 20.0f,
			.radius		= 90.0f,
			.color		= {1.5f, 1.5f, 1.0f},
		};
	}
#endif

	return 0;
}
