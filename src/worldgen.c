#include "worldgen.h"
#include "debug_renderer.h"
#include "color.h"
#include "types.h"
#include "render_targets.h"
#include "util.h"
#include "rng.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <threads.h>

#define CHUNK_COUNT (1)

#define CHUNK_MAX_VERTICES (128 * 1024)

#define WALKER_COUNT (4)
#define WALKER_STEER_PROBABILITY (0.2f)
#define WALKER_FLOOR_MIN_STEPS (64)

enum chunk_state {
	ChunkState_Init = 0,
	ChunkState_RandomWalk,
	ChunkState_CreateMesh,
	ChunkState_Done,
};

typedef struct random_walker {
	bool alive;
	byte id;
	short3 pos;
	direction_t dir;
	uint rng;
	uint counter0;
} random_walker_t;

typedef struct chunk {
	uint32_t			rng;
	enum chunk_state	state;
	uint8_t				connectivity[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
	uint8_t				cornermask[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
	random_walker_t		walkers[WALKER_COUNT];
} chunk_t;

typedef struct worldgen {
	chunk_t			chunks[CHUNK_COUNT];
	//thrd_t			thread;
	VkBuffer		vertexBuffer;

	world_vertex_t*	vertices;
	size_t			vertexCount;

	VkDescriptorSetLayout	descriptorSetLayout;
	VkPipelineLayout		pipelineLayout;
	VkPipeline				pipeline;
	VkPipeline				shadowPipeline;
} worldgen_t;

// static int WorldgenThread(void* args)
// {
// 	worldgen_t* worldgen = (worldgen_t*)args;

// 	while (!worldgen->shouldExit) {
// 		for (int i = 0; i < CHUNK_COUNT; ++i) {
// 			if (worldgen->chunks[i].state == ChunkState_Init)
// 			{

// 			}
// 		}
// 	}

// 	return 0;
// }

static int CreateWorldPipelineLayout(
	worldgen_t* worldgen,
	vulkan_t* vulkan);

static int CreateWorldPipeline(
	VkPipeline* pipeline, 
	vulkan_t* vulkan, 
	VkPipelineLayout pipelineLayout);

static int CreateWorldShadowPipeline(
	VkPipeline* pipeline, 
	vulkan_t* vulkan, 
	VkPipelineLayout pipelineLayout);

static uint32_t LinearCellIndex(short3 pos)
{
	return (pos.z * CHUNK_SIZE_X * CHUNK_SIZE_Y) + (pos.y * CHUNK_SIZE_X) + pos.x;
}

bool IsChunkOutOfBounds(short3 pos)
{
	return (pos.x < 0 || pos.y < 0 || pos.z < 0 || pos.x >= CHUNK_SIZE_X || pos.y >= CHUNK_SIZE_Y || pos.z >= CHUNK_SIZE_Z);
}

static int TearDownWall(chunk_t* chunk, short3 from, direction_t dir)
{
	chunk->connectivity[from.x][from.y][from.z] |= (1 << dir);

	short3 opp = from;
	StepInDirection(&opp, dir);

	if (!IsChunkOutOfBounds(opp)) {
		chunk->connectivity[opp.x][opp.y][opp.z] |= (1 << OppositeDirection(dir));
	}
}

static direction_t RotateLeft(direction_t dir)
{
	if (dir < 4) {
		return (dir + 3) % 4;
	}
	return dir;
}

static direction_t RotateRight(direction_t dir)
{
	if (dir < 4) {
		return (dir + 1) % 4;
	}
	return dir;
}

void StartRandomWalk(random_walker_t* walker, uint8_t id, short3 pos, uint32_t seed)
{
	walker->alive = true;
	walker->id = id;
	walker->pos = pos;
	walker->rng = seed;
	walker->dir = lcg_rand(&walker->rng) % 4;
}

void StopRandomWalk(random_walker_t* walker)
{
	walker->alive = false;
}

void RandomSteer(random_walker_t* walker)
{
	if (lcg_randf(&walker->rng) > .5f)
	{
		walker->dir = RotateLeft(walker->dir);
	}
	else
	{
		walker->dir = RotateRight(walker->dir);
	}
}

static void TickRandomWalk(random_walker_t* walker, chunk_t* chunk)
{
	if (!walker->alive) {
		return;
	}

	if (walker->dir == Direction_Down) {
		walker->dir = lcg_rand(&walker->rng) % 4;
	}
	else
	{
		if (lcg_randf(&walker->rng) < WALKER_STEER_PROBABILITY) {
			RandomSteer(walker);
		}
		else if (walker->counter0 > WALKER_FLOOR_MIN_STEPS) {
			walker->counter0 = 0;
			walker->dir = Direction_Down;
		}

		++walker->counter0;
	}

	short3 next = walker->pos;
	StepInDirection(&next, walker->dir);

	if (next.y < 0)
	{
		StopRandomWalk(walker);
	}
	else if (next.x < 0 || next.z < 0 || next.x >= CHUNK_SIZE_X || next.z >= CHUNK_SIZE_Z)
	{
		walker->dir = OppositeDirection(walker->dir);
	}
	else
	{
		TearDownWall(chunk, walker->pos, walker->dir);

		walker->pos = next;
	}
}

worldgen_t* CreateWorldgen(vulkan_t* vulkan)
{
	int r;
	worldgen_t* worldgen;
	
	worldgen = calloc(1, sizeof(worldgen_t));
	if (worldgen == NULL) {
		return NULL;
	}
	
	CreateWorldPipelineLayout(worldgen, vulkan);
	CreateWorldPipeline(&worldgen->pipeline, vulkan, worldgen->pipelineLayout);
	CreateWorldShadowPipeline(&worldgen->shadowPipeline, vulkan, worldgen->pipelineLayout);
	
	// r = thrd_create(&worldgen->thread, WorldgenThread, worldgen);
	// if (r != 0) {
	// 	DestroyWorldgen(worldgen);
	// 	return NULL;
	// }

	return worldgen;
}

void DestroyWorldgen(worldgen_t* worldgen)
{
}

void GetWorldgenInfo(
	worldgen_info_t* info,
	worldgen_t* worldgen)
{
	info->pipelineLayout	= worldgen->pipelineLayout;
	info->pipeline			= worldgen->pipeline;
	info->shadowPipeline	= worldgen->shadowPipeline;
	info->vertexBuffer		= worldgen->vertexBuffer;
	info->vertexCount		= worldgen->vertexCount;
}

int AllocateWorldgenStagingMemory(staging_memory_allocator_t* allocator, worldgen_t* worldgen)
{
	PushStagingBufferAllocation(allocator, &worldgen->vertexBuffer, (void**)&worldgen->vertices, CHUNK_MAX_VERTICES * sizeof(world_vertex_t), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "World Vertices");
	return 0;
}

int TickChunk_Init(chunk_t* chunk)
{
	memset(chunk->connectivity, 0, sizeof(chunk->connectivity));
	memset(chunk->cornermask, 0, sizeof(chunk->cornermask));

	chunk->rng = time(NULL);

	for (int i = 0; i < WALKER_COUNT; ++i)
	{
		const short3 p = {
			lcg_rand(&chunk->rng) % CHUNK_SIZE_X,
			CHUNK_SIZE_Y - 1,
			lcg_rand(&chunk->rng) % CHUNK_SIZE_Z,
		};
		StartRandomWalk(&chunk->walkers[i], i, p, lcg_rand(&chunk->rng));
	}

	++chunk->state;
	return 0;
}

int TickChunk_RandomWalk(chunk_t* chunk)
{
	bool anyAlive = false;

	for (int i = 0; i < WALKER_COUNT; ++i)
	{
		random_walker_t* walker = &chunk->walkers[i];
		TickRandomWalk(walker, chunk);
		
		if (walker->alive) {
			anyAlive = true;
		}
	}

	bool allDead = !anyAlive;
	if (allDead) {
		++chunk->state;
		return 0;
	}

	return 0;
}

typedef struct meshing_context {
	world_vertex_t*	vertices;
	size_t			vertexCount;
} meshing_context_t;

world_vertex_t* AllocTriangle(meshing_context_t* ctx)
{
	const size_t i = ctx->vertexCount;
	ctx->vertexCount += 3;
	return &ctx->vertices[i];
}

void AddTriangle(meshing_context_t* ctx, ushort3 a, ushort3 b, ushort3 c, uint8_t dmask)
{
	world_vertex_t* v = AllocTriangle(ctx);
	v[0] = (world_vertex_t){a.x, a.y, a.z, dmask};
	v[1] = (world_vertex_t){b.x, b.y, b.z, dmask};
	v[2] = (world_vertex_t){c.x, c.y, c.z, dmask};
}

void AddQuad(meshing_context_t* ctx, ushort3 a, ushort3 b, ushort3 c, ushort3 d, uint8_t dmask)
{
	AddTriangle(ctx, a, b, c, dmask);
	AddTriangle(ctx, c, b, d, dmask);
}

void AddPaintingCanvas(meshing_context_t* ctx, ushort3 topleft, ushort2 size, direction_t dir)
{
	ushort3 q[4];
	
	switch (dir)
	{
	case Direction_South:
		q[0] = (ushort3){ topleft.x,			topleft.y,			topleft.z };
		q[1] = (ushort3){ topleft.x + size.x,	topleft.y,			topleft.z };
		q[2] = (ushort3){ topleft.x,			topleft.y + size.y,	topleft.z };
		q[3] = (ushort3){ topleft.x + size.x,	topleft.y + size.y,	topleft.z };
		break;
	case Direction_North:
		q[0] = (ushort3){ topleft.x + size.x,	topleft.y,			topleft.z };
		q[1] = (ushort3){ topleft.x,			topleft.y,			topleft.z };
		q[2] = (ushort3){ topleft.x + size.x,	topleft.y + size.y,	topleft.z };
		q[3] = (ushort3){ topleft.x,			topleft.y + size.y,	topleft.z };
		break;
	case Direction_East:
		q[0] = (ushort3){ topleft.x,			topleft.y,			topleft.z + size.x };
		q[1] = (ushort3){ topleft.x,			topleft.y,			topleft.z };
		q[2] = (ushort3){ topleft.x,			topleft.y + size.y,	topleft.z + size.x };
		q[3] = (ushort3){ topleft.x,			topleft.y + size.y,	topleft.z };
		break;
	case Direction_West:
		q[0] = (ushort3){ topleft.x,			topleft.y,			topleft.z };
		q[1] = (ushort3){ topleft.x,			topleft.y,			topleft.z + size.x };
		q[2] = (ushort3){ topleft.x,			topleft.y + size.y,	topleft.z };
		q[3] = (ushort3){ topleft.x,			topleft.y + size.y,	topleft.z + size.x };
		break;
	default:
		assert(0);
	}

	const uint16_t normal = 1 << dir;

	world_vertex_t* v;

	const uint32_t pid = rand() & 0xf;
	
	v = AllocTriangle(ctx);
	v[0] = (world_vertex_t){ q[0], normal, (1<<31) | (0 << 4) | (0 << 5) | pid };
	v[1] = (world_vertex_t){ q[1], normal, (1<<31) | (1 << 4) | (0 << 5) | pid };
	v[2] = (world_vertex_t){ q[2], normal, (1<<31) | (0 << 4) | (1 << 5) | pid };

	v = AllocTriangle(ctx);
	v[0] = (world_vertex_t){ q[2], normal, (1<<31) | (0 << 4) | (1 << 5) | pid };
	v[1] = (world_vertex_t){ q[1], normal, (1<<31) | (1 << 4) | (0 << 5) | pid };
	v[2] = (world_vertex_t){ q[3], normal, (1<<31) | (1 << 4) | (1 << 5) | pid };
}

int TickChunk_CreateMesh(chunk_t* chunk, worldgen_t* worldgen)
{
	//worldgen->vertexCount = 0;

	meshing_context_t ctx = {
		.vertices = worldgen->vertices,
	};

	const uint16_t sx = (0xffffu / CHUNK_SIZE_X);
	const uint16_t sy = (0xffffu / CHUNK_SIZE_Y);
	const uint16_t sz = (0xffffu / CHUNK_SIZE_Z);

	const uint16_t bx = sx / 16u;
	const uint16_t by = sy / 16u;
	const uint16_t bz = sz / 16u;

	//printf("{%d, %d, %d}\n", sx, sy, sz);

	for (uint16_t x = 0; x < CHUNK_SIZE_X-1; ++x)
	{
		for (uint16_t y = 0; y < CHUNK_SIZE_Y; ++y)
		{
			for (uint16_t z = 0; z < CHUNK_SIZE_Z-1; ++z)
			{
				const uint8_t mask = chunk->connectivity[x][y][z];
				if (mask == 0) {
					continue;
				}

				uint8_t nmask[4];
				//nmask[Direction_North]	= chunk->connectivity[x][y][z-1];
				nmask[Direction_East]	= chunk->connectivity[x+1][y][z];
				nmask[Direction_South]	= chunk->connectivity[x][y][z+1];
				//nmask[Direction_West]	= chunk->connectivity[x-1][y][z];
				
				if (nmask[Direction_South] != 0) {
					if (((mask & DMASK_S) + (nmask[Direction_South] & DMASK_N)) == 0) {
						TearDownWall(chunk, (short3){x,y,z}, Direction_South);
					}
				}
				if (nmask[Direction_East] != 0) {
					if (((mask & DMASK_E) + (nmask[Direction_East] & DMASK_W)) == 0) {
						TearDownWall(chunk, (short3){x,y,z}, Direction_East);
					}
				}
			}
		}
	}
	
	for (uint16_t z = 0; z < CHUNK_SIZE_Z; ++z)
	{
		for (uint16_t y = 0; y < CHUNK_SIZE_Y; ++y)
		{
			for (uint16_t x = 0; x < CHUNK_SIZE_X; ++x)
			{
				const uint8_t mask = chunk->connectivity[x][y][z];
				if (mask == 0) {
					continue;
				}

				uint8_t nmask[4];
				nmask[Direction_North]	= (z > 0) ? chunk->connectivity[x][y][z-1] : 0;
				nmask[Direction_East]	= (x < CHUNK_SIZE_X-1) ? chunk->connectivity[x+1][y][z] : 0;
				nmask[Direction_South]	= (z < CHUNK_SIZE_Z-1) ? chunk->connectivity[x][y][z+1] : 0;
				nmask[Direction_West]	= (x > 0) ? chunk->connectivity[x-1][y][z] : 0;

				uint8_t cornerMask = 0;

				if ((mask & DMASK_NE) == DMASK_NE)
				{
					if ((nmask[Direction_North] & DMASK_E) == 0 ||
						(nmask[Direction_East]  & DMASK_N) == 0)
					{
						cornerMask |= DMASK_N;
					}
				}
				if ((mask & DMASK_SE) == DMASK_SE)
				{
					if ((nmask[Direction_South] & DMASK_E) == 0 ||
						(nmask[Direction_East]  & DMASK_S) == 0)
					{
						cornerMask |= DMASK_E;
					}
				}
				if ((mask & DMASK_SW) == DMASK_SW)
				{
					if ((nmask[Direction_South] & DMASK_W) == 0 ||
						(nmask[Direction_West]  & DMASK_S) == 0)
					{
						cornerMask |= DMASK_S;
					}
				}
				if ((mask & DMASK_NW) == DMASK_NW)
				{
					if ((nmask[Direction_North] & DMASK_W) == 0 ||
						(nmask[Direction_West]  & DMASK_N) == 0)
					{
						cornerMask |= DMASK_W;
					}
				}

				chunk->cornermask[x][y][z] = cornerMask;
			}
		}
	}

	for (uint16_t z = 0; z < CHUNK_SIZE_Z; ++z)
	{
		for (uint16_t y = 0; y < CHUNK_SIZE_Y; ++y)
		{
			for (uint16_t x = 0; x < CHUNK_SIZE_X; ++x)
			{
				const uint8_t mask = chunk->connectivity[x][y][z];
				
				ushort3 corners[8];
				corners[0] = (ushort3){(x+0) * sx, (y+0) * sy, (z+0) * sz};
				corners[1] = (ushort3){(x+1) * sx, (y+0) * sy, (z+0) * sz};
				corners[2] = (ushort3){(x+0) * sx, (y+1) * sy, (z+0) * sz};
				corners[3] = (ushort3){(x+1) * sx, (y+1) * sy, (z+0) * sz};
				corners[4] = (ushort3){(x+0) * sx, (y+0) * sy, (z+1) * sz};
				corners[5] = (ushort3){(x+1) * sx, (y+0) * sy, (z+1) * sz};
				corners[6] = (ushort3){(x+0) * sx, (y+1) * sy, (z+1) * sz};
				corners[7] = (ushort3){(x+1) * sx, (y+1) * sy, (z+1) * sz};

				if (mask != 0) {
					if ((mask & (1 << Direction_North)) == 0) {
						corners[0].z += bz;
						corners[1].z += bz;
						corners[2].z += bz;
						corners[3].z += bz;
					}
					if ((mask & (1 << Direction_South)) == 0) {
						corners[4].z -= bz;
						corners[5].z -= bz;
						corners[6].z -= bz;
						corners[7].z -= bz;
					}
					if ((mask & (1 << Direction_West)) == 0) {
						corners[0].x += bx;
						corners[2].x += bx;
						corners[4].x += bx;
						corners[6].x += bx;
					}
					if ((mask & (1 << Direction_East)) == 0) {
						corners[1].x -= bx;
						corners[3].x -= bx;
						corners[5].x -= bx;
						corners[7].x -= bx;
					}
					if ((mask & (1 << Direction_Up)) == 0) {
						corners[2].y -= by;
						corners[3].y -= by;
						corners[6].y -= by;
						corners[7].y -= by;
					}
					
					if ((mask & (1 << Direction_North)) == 0) {
						AddQuad(&ctx, corners[0], corners[1], corners[2], corners[3], DMASK_S);
					}
					if ((mask & (1 << Direction_South)) == 0) {
						AddQuad(&ctx, corners[5], corners[4], corners[7], corners[6], DMASK_N);
					}

					if ((mask & (1 << Direction_West)) == 0) {
						AddQuad(&ctx, corners[0], corners[2], corners[4], corners[6], DMASK_E);
					}
					if ((mask & (1 << Direction_East)) == 0) {
						AddQuad(&ctx, corners[3], corners[1], corners[7], corners[5], DMASK_W);
					}

					if ((mask & (1 << Direction_Down)) == 0) {
						AddQuad(&ctx, corners[1], corners[0], corners[5], corners[4], DMASK_U);
					}
					if ((mask & (1 << Direction_Up)) == 0) {
						AddQuad(&ctx, corners[2], corners[3], corners[6], corners[7], DMASK_D);
					}

					ushort2 ps = {300 + (rand()%1000), 1300 + (rand()%1000)};
					int py = (rand()%2000)-1000;

					if ((mask & DMASK_N) == 0 && (rand() & 7) == 0) {
						AddPaintingCanvas(&ctx, (ushort3){ (x*sx)+(sx/2) - ps.x/2, py + (y*sy)+(sy/2) - ps.y/2, (z*sz+bz+1) }, ps, Direction_South);
					}
					if ((mask & DMASK_S) == 0 && (rand() & 7) == 0) {
						AddPaintingCanvas(&ctx, (ushort3){ (x*sx)+(sx/2) - ps.x/2, py + (y*sy)+(sy/2) - ps.y/2, (z*sz+sz-bz-1) }, ps, Direction_North);
					}
					if ((mask & DMASK_W) == 0 && (rand() & 7) == 0) {
						AddPaintingCanvas(&ctx, (ushort3){ (x*sx+bx+1), py + (y*sy)+(sy/2) - ps.y/2, (z*sz)+(sz/2) - ps.x/2 }, ps, Direction_East);
					}
					if ((mask & DMASK_E) == 0 && (rand() & 7) == 0) {
						AddPaintingCanvas(&ctx, (ushort3){ (x*sx+sx-bx-1), py + (y*sy)+(sy/2) - ps.y/2, (z*sz)+(sz/2) - ps.x/2 }, ps, Direction_West);
					}
				}

				const uint8_t cornerMask = chunk->cornermask[x][y][z];
				ushort3 q[4];
				
				if (cornerMask & DMASK_N) { // NE
					q[0] = corners[3]; q[0].x -= bx;
					q[1] = corners[1]; q[1].x -= bx;
					q[2] = corners[3]; q[2].z += bz;
					q[3] = corners[1]; q[3].z += bz;
					AddQuad(&ctx, q[0], q[1], q[2], q[3], DMASK_SW);
				}
				if (cornerMask & DMASK_E) { // SE
					q[0] = corners[5]; q[0].x -= bx;
					q[1] = corners[7]; q[1].x -= bx;
					q[2] = corners[5]; q[2].z -= bz;
					q[3] = corners[7]; q[3].z -= bz;
					AddQuad(&ctx, q[0], q[1], q[2], q[3], DMASK_NW);
				}
				if (cornerMask & DMASK_S) { // SW
					q[0] = corners[6]; q[0].x += bx;
					q[1] = corners[4]; q[1].x += bx;
					q[2] = corners[6]; q[2].z -= bz;
					q[3] = corners[4]; q[3].z -= bz;
					AddQuad(&ctx, q[0], q[1], q[2], q[3], DMASK_NE);
				}
				if (cornerMask & DMASK_W) { // NW
					q[0] = corners[0]; q[0].x += bx;
					q[1] = corners[2]; q[1].x += bx;
					q[2] = corners[0]; q[2].z += bz;
					q[3] = corners[2]; q[3].z += bz;
					AddQuad(&ctx, q[0], q[1], q[2], q[3], DMASK_SE);
				}
			}
		}
	}

	worldgen->vertexCount = ctx.vertexCount;

	++chunk->state;
	return 0;
}

int TickWorldgen(worldgen_t* worldgen)
{
	for (int i = 0; i < CHUNK_COUNT; ++i)
	{
		chunk_t* chunk = &worldgen->chunks[i];
		switch (chunk->state) {
			case ChunkState_Init:
				return TickChunk_Init(chunk);
			case ChunkState_RandomWalk:
				return TickChunk_RandomWalk(chunk);
			case ChunkState_CreateMesh:
				return TickChunk_CreateMesh(chunk, worldgen);
			case ChunkState_Done:
				break;
		}
	}

	return 0;
}

static void DebugVisualizeChunk(chunk_t* chunk)
{
	for (int z = 0; z < CHUNK_SIZE_Z; ++z)
	{
		for (int y = 0; y < CHUNK_SIZE_Y; ++y)
		{
			for (int x = 0; x < CHUNK_SIZE_X; ++x)
			{
				const uint8_t mask = chunk->connectivity[x][y][z];
				if (mask == 0) {
					continue;
				}

				vec3 p = {x, y, z};
				vec3 p1 = {x + 1, y + 1, z + 1};

				p.x *= VOXEL_SIZE_X;
				p.y *= VOXEL_SIZE_Y;
				p.z *= VOXEL_SIZE_Z;

				p1.x *= VOXEL_SIZE_X;
				p1.y *= VOXEL_SIZE_Y;
				p1.z *= VOXEL_SIZE_Z;
				
				DrawDebugBox(p, p1, COLOR_RED | COLOR_GREEN);
			}
		}
	}
}

void DebugVisualizeWorldgen(worldgen_t* worldgen)
{
	DebugVisualizeChunk(&worldgen->chunks[0]);
}

VkDescriptorSet CreateWorldDescriptorSet(
	worldgen_t* worldgen,
	descriptor_allocator_t* dsalloc,
	VkDescriptorBufferInfo frameUniformBuffer,
	VkDescriptorBufferInfo pointLightBuffer,
	VkDescriptorImageInfo paintingImage,
	VkDescriptorImageInfo pointShadowAtlas)
{
	StartBindingDescriptors(dsalloc, worldgen->descriptorSetLayout, "World");
	BindUniformBuffer(dsalloc, 0, frameUniformBuffer);
	BindStorageBuffer(dsalloc, 1, pointLightBuffer);
	BindCombinedImageSampler(dsalloc, 2, paintingImage);
	BindCombinedImageSampler(dsalloc, 3, pointShadowAtlas);
	return FinishBindingDescriptors(dsalloc);
}

void DrawWorld(
	VkCommandBuffer cb, 
	worldgen_t* worldgen,
	VkDescriptorSet descriptorSet)
{
	if (worldgen->vertexCount == 0)
	{
		return;
	}

	const VkDeviceSize vertexBufferOffset = 0;

	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, worldgen->pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, worldgen->pipeline);
	vkCmdBindVertexBuffers(cb, 0, 1, &worldgen->vertexBuffer, &vertexBufferOffset);
	vkCmdDraw(cb, worldgen->vertexCount, 1, 0, 0);
}

int GetWorldConnectivity(
	uint8_t connectivity[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z],
	worldgen_t* worldgen)
{
	chunk_t* chunk = &worldgen->chunks[0];
	if (chunk->state != ChunkState_Done) {
		return 1;
	}

	memcpy(connectivity, chunk->connectivity, sizeof(chunk->connectivity));
	return 0;
}

static int CreateWorldPipelineLayout(
	worldgen_t* worldgen,
	vulkan_t* vulkan)
{
	const VkDescriptorSetLayoutBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
	};
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = countof(bindings),
		.pBindings = bindings,
	};
	if (vkCreateDescriptorSetLayout(vulkan->device, &descriptorSetLayoutInfo, NULL, &worldgen->descriptorSetLayout) != VK_SUCCESS) {
		return 1;
	}
	SetDescriptorSetLayoutName(vulkan, worldgen->descriptorSetLayout, "World");

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &worldgen->descriptorSetLayout,
	};
	if (vkCreatePipelineLayout(vulkan->device, &pipelineLayoutInfo, NULL, &worldgen->pipelineLayout) != VK_SUCCESS) {
		return 1;
	}
	SetPipelineLayoutName(vulkan, worldgen->pipelineLayout, "World");

	return 0;
}

static int CreateWorldPipeline(
	VkPipeline* pipeline, 
	vulkan_t* vulkan, 
	VkPipelineLayout pipelineLayout)
{
	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = g_shaders.modules[SHADER_WORLD_VERT],
			.pName = "main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = g_shaders.modules[SHADER_WORLD_FRAG],
			.pName = "main",
		},
	};

	const VkVertexInputBindingDescription vertexBindings[] = {
		{
			.binding = 0,
			.stride = sizeof(world_vertex_t),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		}
	};

	const VkVertexInputAttributeDescription vertexAttributes[] = {
		{
			.binding = 0,
			.location = 0,
			.format = VK_FORMAT_R16G16B16_UNORM,
		},
		{
			.binding = 0,
			.location = 1,
			.format = VK_FORMAT_R16_UINT,
			.offset = offsetof(world_vertex_t, normal),
		},
		{
			.binding = 0,
			.location = 2,
			.format = VK_FORMAT_R32_UINT,
			.offset = offsetof(world_vertex_t, flags0),
		},
		{
			.binding = 0,
			.location = 3,
			.format = VK_FORMAT_R32_UINT,
			.offset = offsetof(world_vertex_t, flags1),
		},
	};

	const VkPipelineVertexInputStateCreateInfo vertexInput = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = countof(vertexBindings),
		.pVertexBindingDescriptions = vertexBindings,
		.vertexAttributeDescriptionCount = countof(vertexAttributes),
		.pVertexAttributeDescriptions = vertexAttributes,
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssembler = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	const VkPipelineRasterizationStateCreateInfo rasterizer = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.lineWidth = 1.0f,
	};

	const VkPipelineMultisampleStateCreateInfo multisampling = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	const VkPipelineColorBlendAttachmentState blendAttachments[] = {
		{
			.blendEnable = VK_TRUE,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
		},
	};

	const VkPipelineColorBlendStateCreateInfo colorBlending = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = blendAttachments,
	};

	const VkPipelineViewportStateCreateInfo viewportState = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	const VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	
	const VkPipelineDynamicStateCreateInfo dynamicState = {
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = countof(dynamicStates),
		.pDynamicStates = dynamicStates,
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		.depthWriteEnable = VK_TRUE,
	};

	const VkFormat colorFormats[] = { SCENE_COLOR_FORMAT };
	const VkPipelineRenderingCreateInfo renderingInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = countof(colorFormats),
		.pColorAttachmentFormats = colorFormats,
		.depthAttachmentFormat = SCENE_DEPTH_FORMAT,
	};

	const VkGraphicsPipelineCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderingInfo,
		.stageCount = countof(stages),
		.pStages = stages,
		.layout = pipelineLayout,
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembler,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pColorBlendState = &colorBlending,
		.pViewportState = &viewportState,
		.pDynamicState = &dynamicState,
		.pDepthStencilState = &depthStencilState,
	};

	if (vkCreateGraphicsPipelines(vulkan->device, NULL, 1, &createInfo, NULL, pipeline) != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines failed\n");
		return 1;
	}

	return 0;
}

static int CreateWorldShadowPipeline(
	VkPipeline* pipeline, 
	vulkan_t* vulkan, 
	VkPipelineLayout pipelineLayout)
{
	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = g_shaders.modules[SHADER_WORLD_SHADOW_VERT],
			.pName = "main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_GEOMETRY_BIT,
			.module = g_shaders.modules[SHADER_WORLD_SHADOW_GEOM],
			.pName = "main",
		},
	};

	const VkVertexInputBindingDescription vertexBindings[] = {
		{
			.binding = 0,
			.stride = sizeof(world_vertex_t),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		}
	};

	const VkVertexInputAttributeDescription vertexAttributes[] = {
		{
			.binding = 0,
			.location = 0,
			.format = VK_FORMAT_R16G16B16_UNORM,
		},
	};

	const VkPipelineVertexInputStateCreateInfo vertexInput = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = countof(vertexBindings),
		.pVertexBindingDescriptions = vertexBindings,
		.vertexAttributeDescriptionCount = countof(vertexAttributes),
		.pVertexAttributeDescriptions = vertexAttributes,
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssembler = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	const VkPipelineRasterizationStateCreateInfo rasterizer = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.lineWidth = 1.0f,
		.depthBiasEnable = VK_TRUE,
		.depthBiasConstantFactor = 1.5f,
		.depthBiasSlopeFactor = 2.0f,
	};

	const VkPipelineMultisampleStateCreateInfo multisampling = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	const VkPipelineColorBlendAttachmentState blendAttachments[] = {
		{
			.blendEnable = VK_TRUE,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
		},
	};

	const VkPipelineColorBlendStateCreateInfo colorBlending = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = blendAttachments,
	};

	const VkPipelineViewportStateCreateInfo viewportState = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	const VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	
	const VkPipelineDynamicStateCreateInfo dynamicState = {
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = countof(dynamicStates),
		.pDynamicStates = dynamicStates,
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		.depthWriteEnable = VK_TRUE,
	};

	const VkPipelineRenderingCreateInfo renderingInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.depthAttachmentFormat = VK_FORMAT_D16_UNORM,
	};

	const VkGraphicsPipelineCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderingInfo,
		.stageCount = countof(stages),
		.pStages = stages,
		.layout = pipelineLayout,
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembler,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pColorBlendState = &colorBlending,
		.pViewportState = &viewportState,
		.pDynamicState = &dynamicState,
		.pDepthStencilState = &depthStencilState,
	};

	if (vkCreateGraphicsPipelines(vulkan->device, NULL, 1, &createInfo, NULL, pipeline) != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines failed\n");
		return 1;
	}

	return 0;
}