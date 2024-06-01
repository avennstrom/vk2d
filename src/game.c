#include "game.h"
#include "mat.h"
#include "worldgen.h"
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
	GameState_AwaitWorld = 0,
	GameState_PlayerSpawn,
	GameState_EnemySpawn,
	GameState_Play,
};

typedef struct enemy {
	short3		pos;
	short3		prev;
	direction_t	dir;
	float3		fpos;
	float		t;
} enemy_t;

typedef struct game {
	window_t*	window;
	worldgen_t*	worldgen;
	int			state;
	bool		lockMouse;
	uint8_t		connectivity[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

	player_t	player;
	float3		playerSpawn;

	enemy_t		enemies[MAX_ENEMIES];
	size_t		enemyCount;

	bool		keystate[256];
	bool		buttonstate[2];
} game_t;

static enemy_t* SpawnEnemies(game_t* game, size_t count)
{
	const size_t rem = MAX_ENEMIES - game->enemyCount;
	if (count > rem) {
		return NULL;
	}

	enemy_t* e = game->enemies + game->enemyCount;
	game->enemyCount += count;

	return e;
}

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

game_t* game_create(window_t* window, worldgen_t* worldgen)
{
	game_t* game = calloc(1, sizeof(game_t));
	if (game == NULL) {
		return NULL;
	}

	game->window	= window;
	game->worldgen	= worldgen;

	game->player.flashlight = true;

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
				setMouseLock(game->window, game->lockMouse);
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

static void StepEnemy(game_t* game, enemy_t* enemy)
{
	StepInDirection(&enemy->pos, enemy->dir);

	const byte mask = game->connectivity[enemy->pos.x][enemy->pos.y][enemy->pos.z];
	assert(mask != 0);

	for (;;) {
		if ((mask & (1 << enemy->dir)) == 0)
		{
			if (rand() & 1) {
				enemy->dir = (enemy->dir + 1) % 4;
			}
			else {
				enemy->dir = (enemy->dir + 3) % 4;
			}
			continue;
		}
		
		break;
	}
}

static void TickEnemies(game_t* game, float deltaTime)
{
	for (size_t i = 0; i < game->enemyCount; ++i) {
		enemy_t* enemy = game->enemies + i;
		enemy->t += deltaTime * 0.0005f;
		if (enemy->t >= 1.0f) {
			enemy->t -= floorf(enemy->t); // only step once if big delta
			StepEnemy(game, enemy);
		}
		
		short3 o = {0,0,0};
		StepInDirection(&o, enemy->dir);

		

		enemy->fpos.x = (enemy->pos.x+enemy->t*o.x)*VOXEL_SIZE_X + VOXEL_SIZE_X*0.5f;
		enemy->fpos.y = (enemy->pos.y+enemy->t*o.y)*VOXEL_SIZE_Y + VOXEL_SIZE_Y*0.5f;
		enemy->fpos.z = (enemy->pos.z+enemy->t*o.z)*VOXEL_SIZE_Z + VOXEL_SIZE_Z*0.5f;
	}
}

static void FindPlayerSpawn(short3* pos, game_t* game)
{
	uint rng = rand();

	for (;;) {
		const short3 candidate = { 
			lcg_rand(&rng) % CHUNK_SIZE_X,
			CHUNK_SIZE_Y - 1,
			lcg_rand(&rng) % CHUNK_SIZE_Z,
		};
		const uint8_t mask = game->connectivity[candidate.x][candidate.y][candidate.z];
		if (mask == 0) {
			continue;
		}
		
		*pos = candidate;
		return;
	}
}

void TickGame(game_t* game, float deltaTime)
{
	int r;

	if (game->state == GameState_AwaitWorld) {
		r = GetWorldConnectivity(game->connectivity, game->worldgen);
		if (r != 0) {
			//printf("Worldgen not done.\n");
			return;
		}

		++game->state;
	}

	if (game->state == GameState_PlayerSpawn) {
		short3 playerSpawn;
		FindPlayerSpawn(&playerSpawn, game);

		//playerSpawn = (ushort3){0,0,0};

		printf("player spawn: %d, %d, %d\n", playerSpawn.x, playerSpawn.y, playerSpawn.z);

		game->player.pos.x = playerSpawn.x*VOXEL_SIZE_X + VOXEL_SIZE_X*0.5f;
		game->player.pos.y = playerSpawn.y*VOXEL_SIZE_Y;
		game->player.pos.z = playerSpawn.z*VOXEL_SIZE_Z + VOXEL_SIZE_Z*0.5f;

		game->playerSpawn = game->player.pos;
		game->playerSpawn.y += 1.0f;

		++game->state;
	}

	if (game->state == GameState_EnemySpawn)
	{
		enemy_t* enemies = SpawnEnemies(game, MAX_ENEMIES);
		for (size_t i = 0; i < MAX_ENEMIES; ++i)
		{
			enemy_t* enemy = enemies + i;
			enemy->t = 0.0f;
			FindPlayerSpawn(&enemy->pos, game);
			const byte mask = game->connectivity[enemy->pos.x][enemy->pos.y][enemy->pos.z];
			if (mask & DMASK_N) {
				enemy->dir = Direction_North;
			}
			else if (mask & DMASK_E) {
				enemy->dir = Direction_East;
			}
			else if (mask & DMASK_S) {
				enemy->dir = Direction_South;
			}
			else if (mask & DMASK_W) {
				enemy->dir = Direction_West;
			}
		}

		++game->state;
	}

	if (game->state == GameState_Play) {
		TickEnemies(game, deltaTime);
		TickPlayerMovement(game, deltaTime);
	}

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

	//point_light = scb_add_point_lights(scb, game->enemyCount);
	spot_light = scb_add_spot_lights(scb, game->enemyCount);
	for (size_t i = 0; i < game->enemyCount; ++i) {
		enemy_t* enemy = game->enemies + i;
		// point_light[i] = (scb_point_light_t){
		// 	.position = enemy->fpos,
		// 	.radius = 8.0f,
		// 	.color = {8.0f, 3.0f, 2.0f},
		// };
		short3 n = {};
		StepInDirection(&n, enemy->dir);

		mat4 m = mat_identity();
		m = mat_translate(m, enemy->fpos);

		spot_light[i] = (scb_spot_light_t){
			.transform = m,
			.range = 32.0f,
			.radius = 90.0f,
			.color = {5.0f, 0.2f, 0.0f},
		};
	}

	return 0;
}
