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
#include <float.h>

#define PLAYER_SPEED		0.008f
#define PLAYER_SPEED_BOOST	4.0f
#define PLAYER_GRAVITY		0.00005f
#define PLAYER_JUMP_VEL		0.015f

typedef struct camera {
	vec2	pos;
	float	height;
} camera_t;

typedef struct player {
	vec2	pos;
	vec2	vel;
	vec2	size;
	bool	isGrounded;
} player_t;

enum {
	GameState_LoadScene = 0,
	GameState_Play,
};

typedef struct game {
	world_t*				world;
	window_t*				window;
	const model_loader_t*	modelLoader;
	int						state;
	//bool					lockMouse;
	float					aspectRatio;
	int						mouseX;
	int						mouseY;

	float					t;
	player_t				player;
	camera_t				camera;

	bool					keystate[256];
	bool					buttonstate[2];
} game_t;

game_t* game_create(window_t* window, const model_loader_t* modelLoader, world_t* world)
{
	game_t* game = calloc(1, sizeof(game_t));
	if (game == NULL) {
		return NULL;
	}

	game->world			= world;
	game->window		= window;
	game->modelLoader	= modelLoader;

	game->player = (player_t){
		.pos = {2.0f, 0.1f},
		.size = {0.5f, 1.0f},
	};

	game->camera = (camera_t){
		.height = 10.0f,
	};

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
			game->mouseX = event->data.mouse.pos.x;
			game->mouseY = event->data.mouse.pos.y;
			break;
		case WINDOW_EVENT_MOUSE_SCROLL:
			if (event->data.scroll.delta > 0)
			{
				game->camera.height *= 0.7f;
			}
			if (event->data.scroll.delta < 0)
			{
				game->camera.height *= 1.3f;
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

	if (game->keystate[KEY_SPACE] && game->player.isGrounded)
	{
		game->player.vel.y = PLAYER_JUMP_VEL;
	}
}

static void calculate_camera(scb_camera_t* renderCamera, const camera_t* camera, float aspectRatio)
{
	mat4 m = mat_identity();
	m = mat_translate(m, (vec3){ camera->pos.x, camera->pos.y, 0.0f });

	const vec2 viewSize = {camera->height * aspectRatio, camera->height};

	const mat4 viewMatrix = mat_invert(m);
	const mat4 projectionMatrix = mat_orthographic(viewSize, 0.0f, 1.0f);
	const mat4 viewProjectionMatrix = mat_mul(viewMatrix, projectionMatrix);

	// :TODO: transpose these in the scene instead
	renderCamera->transform = mat_transpose(m);
	renderCamera->viewMatrix = mat_transpose(viewMatrix);
	renderCamera->projectionMatrix = mat_transpose(projectionMatrix);
	renderCamera->viewProjectionMatrix = mat_transpose(viewProjectionMatrix);
}

void game_tick(game_t* game, float deltaTime, uint2 resolution)
{
	int r;

	game->aspectRatio = resolution.x / (float)resolution.y;

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

	{
		player_t* player = &game->player;

		player->isGrounded = false;

		player->vel.y -= PLAYER_GRAVITY * deltaTime;
		player->pos = vec2_add(player->pos, vec2_scale(player->vel, deltaTime));

		world_collision_info_t worldCollision;
		world_get_collision_info(&worldCollision, game->world);
		
		for (size_t triangleIndex = 0; triangleIndex < worldCollision.triangleCount; ++triangleIndex)
		{
			const triangle_collider_t* triangle = &worldCollision.triangles[triangleIndex];

			const vec2 corners[4] = 
			{
				{player->pos.x - player->size.x * 0.5f, player->pos.y},
				{player->pos.x + player->size.x * 0.5f, player->pos.y},
				{player->pos.x - player->size.x * 0.5f, player->pos.y + player->size.y},
				{player->pos.x + player->size.x * 0.5f, player->pos.y + player->size.y},
			};
			
			const vec2 edges[][2] = {
				{triangle->a, triangle->b},
				{triangle->b, triangle->c},
				{triangle->c, triangle->a},
			};

			vec3 planes[3];
			
			for (size_t edgeIndex = 0; edgeIndex < countof(edges); ++edgeIndex)
			{
				const vec2 a = edges[edgeIndex][0];
				const vec2 b = edges[edgeIndex][1];

				const vec2 d = vec2_normalize(vec2_sub(b, a));
				const vec2 normal = {-d.y, d.x};
				
#if 0
				const vec2 center = vec2_scale(vec2_add(a, b), 0.5f);
				DrawDebugLine(
					(debug_vertex_t){.x = center.x, .y = center.y, .color = 0xffff00ff},
					(debug_vertex_t){.x = center.x + normal.x * 0.3f, .y = center.y + normal.y * 0.3f, .color = 0xffff00ff}
				);
#endif

				planes[edgeIndex] = (vec3){ normal.x, normal.y, vec2_dot(normal, a) };
			}
			
			vec2 maxPushout;
			float maxPushoutDistance = FLT_MAX;
			
			for (size_t cornerIndex = 0; cornerIndex < countof(corners); ++cornerIndex)
			{
				const vec2 corner = corners[cornerIndex];
				
				float maxDistance = -FLT_MAX;
				uint32_t closestEdge = 0;
				
				uint32_t intersectionCount = 0;
				for (size_t edgeIndex = 0; edgeIndex < countof(edges); ++edgeIndex)
				{
					const vec3 plane = planes[edgeIndex];
					const float d = vec2_dot(vec3_xy(plane), corner) - plane.z;
					if (d < 0.0f)
					{
						++intersectionCount;

						if (d > maxDistance)
						{
							maxDistance	= d;
							closestEdge	= edgeIndex;
						}
					}
				}

				bool hasCollision = intersectionCount == 3;

				const uint32_t color = hasCollision ? 0xff0000ff : 0xff00ffff;
				//DrawDebugPoint((debug_vertex_t){.x = corner.x, .y = corner.y, .color = color});
				
				if (hasCollision)
				{
					const vec3 plane = planes[closestEdge];
					const vec2 n = vec3_xy(plane);
					const vec2 pushout = vec2_scale(n, -maxDistance);
					const vec2 target = vec2_add(corner, pushout);

					// DrawDebugLine(
					// 	(debug_vertex_t){.x = corner.x, .y = corner.y, .color = 0xff0000ff},
					// 	(debug_vertex_t){.x = target.x, .y = target.y, .color = 0xff0000ff}
					// );

					//DrawDebugPoint((debug_vertex_t){.x = target.x, .y = target.y, .color = color});

					if (maxDistance < maxPushoutDistance)
					{
						maxPushoutDistance	= maxDistance;
						maxPushout			= pushout;
					}
				}
			}

			if (maxPushoutDistance != FLT_MAX)
			{
				player->pos			= vec2_add(player->pos, maxPushout);
				player->vel			= (vec2){0.0f, 0.0f};
				player->isGrounded	= true;
			}
		}
	}

	game->camera.pos = vec2_lerp(game->camera.pos, (vec2){game->player.pos.x, game->player.pos.y + game->player.size.y * 0.5f}, 0.005f);

	return;
}

int game_render(scb_t* scb, game_t* game)
{
	scb_point_light_t* point_light;

	calculate_camera(scb_set_camera(scb), &game->camera, game->aspectRatio);

	//DrawDebugCross((vec3){game->player.pos.x, game->player.pos.y}, 1.0f, 0xff0000ff);
	DrawDebugBox(
		(vec3){game->player.pos.x - game->player.size.x / 2.0f, game->player.pos.y},
		(vec3){game->player.pos.x + game->player.size.x / 2.0f, game->player.pos.y + game->player.size.y},
		0xff00ffff
	);

	return 0;
}
