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

#define PLAYER_HEIGHT		1.75f
#define PLAYER_SPEED		0.01f
#define PLAYER_SENSITIVITY	0.0015f

#define MAX_ENEMIES			4

#define MODEL_TANK ((model_handle_t){0})
#define MODEL_COLORTEST ((model_handle_t){1})

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
	float					aspectRatio;
	int						mouseX;
	int						mouseY;

	float					t;
	player_t				player;
	vec3					aimTarget;

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
			game->mouseX = event->data.mouse.x;
			game->mouseY = event->data.mouse.y;
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

static void calculate_camera(scb_camera_t* camera, const player_t* player, float aspectRatio)
{
	mat4 m = mat_identity();
	m = mat_translate(m, GetPlayerHead(player));
	m = mat_rotate_y(m, player->yaw);
	m = mat_rotate_x(m, player->pitch);

	const mat4 viewMatrix = mat_invert(m);
	const mat4 projectionMatrix = mat_perspective(70.0f, aspectRatio, 0.1f, 256.0f);
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
			
			scb_camera_t camera;
			calculate_camera(&camera, &game->player, game->aspectRatio);
			
			const mat4 inverseViewProjection = mat_invert(mat_transpose(camera.viewProjectionMatrix));
			
			vec2 mouseUV = {game->mouseX / (float)viewport->width, game->mouseY / (float)viewport->height};
			mouseUV.y = 1.0f - mouseUV.y;
			mouseUV.x = mouseUV.x * 2.0f - 1.0f;
			mouseUV.y = mouseUV.y * 2.0f - 1.0f;
			
			vec3 near = mat_mul_hom(inverseViewProjection, (vec4){mouseUV.x, mouseUV.y, 0.0f, 1.0f});
			vec3 far = mat_mul_hom(inverseViewProjection, (vec4){mouseUV.x, mouseUV.y, 1.0f, 1.0f});

			vec3 dir = vec3_normalize(vec3_sub(far, near));
			
			float hitDistance;
			if (intersect_ray_plane(&hitDistance, near, dir, (vec4){0.0f, 1.0f, 0.0f, 1.0f}))
			{
				vec3 hitPos = vec3_add(near, vec3_scale(dir, hitDistance));
				DrawDebugCross(hitPos, 1.0f, 0xffffff00);
				
				game->aimTarget = hitPos;
			}
		}
	}

	game->t += deltaTime;

	return;
}

int game_render(scb_t* scb, game_t* game)
{
	scb_point_light_t* point_light;
	scb_spot_light_t* spot_light;

	calculate_camera(scb_set_camera(scb), &game->player, game->aspectRatio);

	vec3 targetPos = {};

	{
		model_hierarchy_t hierarchy;
		mat4 transforms[16];
		mat4 m[16];

		for (int i = 0; i < countof(m); ++i)
		{
			m[i] = mat_identity();
		}

		mat4* m_body = &m[g_model_tank.node_body];
		mat4* m_turret = &m[g_model_tank.node_turret];
		mat4* m_pipe = &m[g_model_tank.node_pipe];

		const vec3 tankPos = (vec3){sin(0.001f * game->t) * 3.0f, 0.0f, 0.0f};
		//const vec3 tankPos = (vec3){0.0f, 0.0f, 0.0f};

		*m_body = mat_translate(*m_body, tankPos);
		
		{
			const vec2 aimVec = vec3_xz(vec3_sub(game->aimTarget, tankPos));
			const float aimAngle = atan2f(-aimVec.y, -aimVec.x);
			*m_turret = mat_rotate_y(*m_turret, aimAngle);
		}

		model_loader_get_model_hierarchy(&hierarchy, game->modelLoader, MODEL_TANK);
		model_hierarchy_resolve(transforms, m, &hierarchy);
		
		{
			const vec3 localAimTarget = mat_mul_vec3(mat_invert(transforms[g_model_tank.node_pipe]), game->aimTarget);
			const float aimAngle = atan2f(localAimTarget.x, localAimTarget.y);
			*m_pipe = mat_rotate_z(*m_pipe, aimAngle);
		}

		model_hierarchy_resolve(transforms, m, &hierarchy);

		scb_draw_model_t* models = scb_draw_models(scb, 2);

		models[0] = (scb_draw_model_t){
			.model = MODEL_TANK,
			.transform[0] = transforms[0],
			.transform[1] = transforms[1],
			.transform[2] = transforms[2],
			.transform[3] = transforms[3],
			.transform[4] = transforms[4],
			.transform[5] = transforms[5],
		};

		for (int i = 0; i < countof(m); ++i)
		{
			m[i] = mat_identity();
		}


		m[0] = mat_translate(m[0], (vec3){ 10.0f, 0.0f, 0.0f });

		model_loader_get_model_hierarchy(&hierarchy, game->modelLoader, MODEL_COLORTEST);
		model_hierarchy_resolve(transforms, m, &hierarchy);

		models[1] = (scb_draw_model_t){
			.model = MODEL_COLORTEST,
			.transform[0] = transforms[0],
			.transform[1] = transforms[1],
			.transform[2] = transforms[2],
			.transform[3] = transforms[3],
			.transform[4] = transforms[4],
			.transform[5] = transforms[5],
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
