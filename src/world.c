#include "world.h"
#include "types.h"
#include "debug_renderer.h"
#include "vec.h"
#include "util.h"
#include "rng.h"

#include <stdlib.h>
#include <math.h>
#include <assert.h>

#define WORLD_MAX_INDEX_COUNT (256 * 1024)
#define WORLD_MAX_VERTEX_COUNT (256 * 1024)
#define WORLD_INDEX_BUFFER_SIZE (WORLD_MAX_INDEX_COUNT * sizeof(uint16_t))
#define WORLD_POSITION_BUFFER_SIZE (WORLD_MAX_VERTEX_COUNT * sizeof(vec3))
#define WORLD_COLOR_BUFFER_SIZE (WORLD_MAX_VERTEX_COUNT * sizeof(uint32_t))
#define WORLD_STAGING_BUFFER_SIZE (WORLD_INDEX_BUFFER_SIZE + WORLD_POSITION_BUFFER_SIZE + WORLD_COLOR_BUFFER_SIZE)

#define WORLD_MAX_TRIANGLE_COLLIDERS 1024

typedef enum world_state
{
	WORLD_STATE_UPLOAD_TRIANGLES,
	WORLD_STATE_DONE,
} world_state_t;

typedef struct world_colliders
{
	uint32_t			triangleCount;
	triangle_collider_t	triangles[WORLD_MAX_TRIANGLE_COLLIDERS];
} world_colliders_t;

typedef struct parallax_layer
{
	editor_polygon_t	polygon;
} parallax_layer_t;

typedef struct world
{
	vulkan_t*			vulkan;
	particles_t*		particles;
	world_state_t		state;
	uint				stagingCounter;

	VkBuffer			indexBuffer;
	VkDeviceMemory		indexBufferMemory;
	VkBuffer			vertexPositionBuffer;
	VkDeviceMemory		vertexPositionBufferMemory;
	VkBuffer			vertexColorBuffer;
	VkDeviceMemory		vertexColorBufferMemory;
	uint32_t			indexCount;

	VkBuffer			stagingBuffer;
	void*				stagingBufferMemory;

	parallax_layer_t	layers[PARALLAX_LAYER_COUNT];
	world_colliders_t	colliders;

	uint				rng;
	float				pollenTimer;
} world_t;

typedef struct triangle
{
	uint32_t i[3];
} triangle_t;

static void triangle_collider_debug_draw(triangle_collider_t* t);
void editor_polygon_debug_draw(editor_polygon_t* p);
static void editor_polygon_triangulate(triangle_t* triangles, size_t* triangleCount, const editor_polygon_t* polygon);

world_t* world_create(vulkan_t* vulkan, particles_t* particles)
{
	world_t* world = calloc(1, sizeof(world_t));
	if (world == NULL)
	{
		return NULL;
	}

	world->vulkan		= vulkan;
	world->particles	= particles;
	
	world->indexBuffer = CreateBuffer(
		&world->indexBufferMemory,
		vulkan,
		WORLD_INDEX_BUFFER_SIZE,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	world->vertexPositionBuffer = CreateBuffer(
		&world->vertexPositionBufferMemory,
		vulkan,
		WORLD_POSITION_BUFFER_SIZE,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	world->vertexColorBuffer = CreateBuffer(
		&world->vertexColorBufferMemory,
		vulkan,
		WORLD_COLOR_BUFFER_SIZE,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i)
	{
		editor_polygon_t* polygon = &world->layers[i].polygon;
		polygon->vertexPosition[polygon->vertexCount++] = (vec2){-5.0f, 1.0f};
		polygon->vertexPosition[polygon->vertexCount++] = (vec2){0.0f, 0.1f};
		polygon->vertexPosition[polygon->vertexCount++] = (vec2){5.0f, 0.1f};
		polygon->vertexPosition[polygon->vertexCount++] = (vec2){10.0f, -0.5f};
		polygon->vertexPosition[polygon->vertexCount++] = (vec2){5.0f, -1.0f};
		polygon->vertexPosition[polygon->vertexCount++] = (vec2){-5.0f, -1.0f};
		polygon->vertexPosition[polygon->vertexCount++] = (vec2){-10.0f, 0.0f};
		polygon->layer = i;
	}

	return world;
}

void world_destroy(world_t* world)
{
	vulkan_t* vulkan = world->vulkan;

	vkDestroyBuffer(vulkan->device, world->stagingBuffer, NULL);

	vkDestroyBuffer(vulkan->device, world->indexBuffer, NULL);
	vkDestroyBuffer(vulkan->device, world->vertexPositionBuffer, NULL);
	vkDestroyBuffer(vulkan->device, world->vertexColorBuffer, NULL);

	vkFreeMemory(vulkan->device, world->indexBufferMemory, NULL);
	vkFreeMemory(vulkan->device, world->vertexPositionBufferMemory, NULL);
	vkFreeMemory(vulkan->device, world->vertexColorBufferMemory, NULL);

	free(world);
}

void world_alloc_staging_mem(staging_memory_allocator_t* allocator, world_t* world)
{
	PushStagingBufferAllocation(
		allocator,
		&world->stagingBuffer,
		&world->stagingBufferMemory,
		WORLD_STAGING_BUFFER_SIZE,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		"World");
}

typedef struct primitive_context
{
	uint16_t*	indices;
	vec3*		positions;
	uint32_t*	colors;
	
	uint32_t	indexCount;
	uint32_t	vertexCount;
} primitive_context_t;

static void grow_grass(primitive_context_t* ctx, vec3 root, vec2 tangent)
{
	ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 0;
	ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 1;
	ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 2;
	ctx->indexCount += 3;

	float width = 0.025f;
	float height = 0.1f + (rand() / (float)RAND_MAX) * 0.5f;

	ctx->positions[ctx->vertexCount + 0] = (vec3){root.x, root.y + height, root.z};
	ctx->positions[ctx->vertexCount + 1] = (vec3){root.x - width * tangent.x, root.y - width * tangent.y, root.z};
	ctx->positions[ctx->vertexCount + 2] = (vec3){root.x + width * tangent.x, root.y + width * tangent.y, root.z};

	uint8_t red = rand() & 0b1111111;

	ctx->colors[ctx->vertexCount + 0] = 0xff00a000 | red;
	ctx->colors[ctx->vertexCount + 1] = 0x00002000 | (red/2);
	ctx->colors[ctx->vertexCount + 2] = 0x00002000 | (red/2);

	ctx->vertexCount += 3;
}

static void grow_flower(primitive_context_t* ctx, vec3 root, vec2 tangent)
{
	float width = 0.01f;
	float height = 0.3f + (rand() / (float)RAND_MAX) * 0.5f;
	float headSize = 0.1f;

	// stem
	{
		ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 0;
		ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 3] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 4] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 5] = ctx->vertexCount + 3;
		ctx->indexCount += 6;

		ctx->positions[ctx->vertexCount + 0] = (vec3){root.x - width, root.y + height, root.z};
		ctx->positions[ctx->vertexCount + 1] = (vec3){root.x + width, root.y + height, root.z};
		ctx->positions[ctx->vertexCount + 2] = (vec3){root.x - width * tangent.x, root.y - width * tangent.y, root.z};
		ctx->positions[ctx->vertexCount + 3] = (vec3){root.x + width * tangent.x, root.y + width * tangent.y, root.z};

		uint8_t red = rand() & 0b11111;

		ctx->colors[ctx->vertexCount + 0] = 0xff007000 | red;
		ctx->colors[ctx->vertexCount + 1] = 0xff007000 | red;
		ctx->colors[ctx->vertexCount + 2] = 0x00001000 | (red/2);
		ctx->colors[ctx->vertexCount + 3] = 0x00001000 | (red/2);

		ctx->vertexCount += 4;
	}

	// head
	{

		ctx->indices[ctx->indexCount + 0] = ctx->vertexCount + 0;
		ctx->indices[ctx->indexCount + 1] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 2] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 3] = ctx->vertexCount + 2;
		ctx->indices[ctx->indexCount + 4] = ctx->vertexCount + 1;
		ctx->indices[ctx->indexCount + 5] = ctx->vertexCount + 3;
		ctx->indexCount += 6;

		ctx->positions[ctx->vertexCount + 0] = (vec3){root.x - headSize * 0.5f, root.y + height + headSize * 0.5f, root.z};
		ctx->positions[ctx->vertexCount + 1] = (vec3){root.x + headSize * 0.5f, root.y + height + headSize * 0.5f, root.z};
		ctx->positions[ctx->vertexCount + 2] = (vec3){root.x - headSize * 0.2f, root.y + height - headSize * 0.5f, root.z};
		ctx->positions[ctx->vertexCount + 3] = (vec3){root.x + headSize * 0.2f, root.y + height - headSize * 0.5f, root.z};

		uint8_t r = rand() & 0xffu;
		uint8_t g = rand() & 0xffu;
		uint8_t b = rand() & 0xffu;

		uint32_t color = r | (g << 8) | (b << 16);
		uint32_t darkColor = (r/2) | ((g/2) << 8) | ((b/2) << 16);

		ctx->colors[ctx->vertexCount + 0] = 0xff000000 | color;
		ctx->colors[ctx->vertexCount + 1] = 0xff000000 | color;
		ctx->colors[ctx->vertexCount + 2] = 0xff000000 | darkColor;
		ctx->colors[ctx->vertexCount + 3] = 0xff000000 | darkColor;
		ctx->vertexCount += 4;
	}
}

static void grow_plant(primitive_context_t* ctx, vec3 root, vec2 tangent)
{
	const int type = rand() % 2;
	if (type == 0)
	{
		grow_grass(ctx, root, tangent);
	}
	else if (type == 1)
	{
		grow_flower(ctx, root, tangent);
	}
}

static void fill_primitive_data(primitive_context_t* ctx, const editor_polygon_t* polygon, int layerIndex)
{
	triangle_t triangles[256];
	size_t triangleCount;
	editor_polygon_triangulate(triangles, &triangleCount, polygon);

	const float depth = world_get_parallax_layer_depth(layerIndex);

	for (size_t i = 0; i < triangleCount; ++i)
	{
		const triangle_t* triangle = &triangles[i];

		ctx->indices[ctx->indexCount + 0] = triangle->i[0] + ctx->vertexCount;
		ctx->indices[ctx->indexCount + 1] = triangle->i[1] + ctx->vertexCount;
		ctx->indices[ctx->indexCount + 2] = triangle->i[2] + ctx->vertexCount;
		ctx->indexCount += 3;
	}

	for (size_t i = 0; i < polygon->vertexCount; ++i)
	{
		const vec2 p = polygon->vertexPosition[i];
		ctx->positions[ctx->vertexCount + i] = (vec3){ p.x, p.y, depth };
		ctx->colors[ctx->vertexCount + i] = 0x001020;
	}

	ctx->vertexCount += polygon->vertexCount;

	for (uint i = 0; i < polygon->vertexCount; ++i)
	{
		const uint j = (i + 1) % polygon->vertexCount;

		const vec2 p0 = polygon->vertexPosition[i];
		const vec2 p1 = polygon->vertexPosition[j];

		const vec2 d = vec2_normalize(vec2_sub(p1, p0));
		const vec2 n = {-d.y, d.x};
		
		if (n.y <= 0.0f)
		{
			continue;
		}

		const float len = vec2_length(vec2_sub(p1, p0));
		const float plantDensity = 40.0f;

		const int plantCount = len * plantDensity;
		
		for (int i = 0; i < plantCount; ++i)
		{
			const float t = (rand() / (float)RAND_MAX);
			const vec2 p = vec2_lerp(p0, p1, t);
			grow_plant(ctx, (vec3){p.x, p.y, depth}, d);
		}
	}
}

void world_tick(world_t* world)
{
	world->pollenTimer += DELTA_TIME_MS;
	if (world->pollenTimer > 20.0f)
	{
		world->pollenTimer -= 20.0f;

		const editor_polygon_t* polygon = &world->layers[0].polygon;

		const uint vertexIndex = lcg_rand(&world->rng) % polygon->vertexCount;

		const vec2 a = polygon->vertexPosition[vertexIndex];
		const vec2 b = polygon->vertexPosition[(vertexIndex + 1) % polygon->vertexCount];

		const vec2 d = vec2_normalize(vec2_sub(b, a));
		const vec2 n = {-d.y, d.x};
		
		if (n.y > 0.0f)
		{
			const float t = lcg_randf(&world->rng);

			particles_spawn(world->particles, PARTICLE_EFFECT_AMBIENT_POLLEN, (particle_spawn_t){
				.pos = vec2_lerp(a, b, t),
			});
		}
	}
}

void world_update(world_t* world, VkCommandBuffer cb, const render_context_t* rc)
{
	{
		editor_polygon_t* polygon = &world->layers[0].polygon;
		world_colliders_t* colliders = &world->colliders;

		triangle_t triangles[256];
		size_t triangleCount;
		editor_polygon_triangulate(triangles, &triangleCount, polygon);
		
		for (size_t i = 0; i < triangleCount; ++i)
		{
			const uint i0 = triangles[i].i[0];
			const uint i1 = triangles[i].i[1];
			const uint i2 = triangles[i].i[2];
			
			const vec2 p0 = polygon->vertexPosition[i0];
			const vec2 p1 = polygon->vertexPosition[i1];
			const vec2 p2 = polygon->vertexPosition[i2];

			colliders->triangles[i] = (triangle_collider_t){ p0, p1, p2 };
		}

		colliders->triangleCount = triangleCount;
	}

	switch (world->state)
	{
		case WORLD_STATE_UPLOAD_TRIANGLES:
		{
			if (world->stagingCounter++ < FRAME_COUNT)
			{
				break;
			}
			world->stagingCounter = 0;

			uint16_t* stagingIndices = (uint16_t*)world->stagingBufferMemory;
			vec3* stagingPositions = (vec3*)((uint8_t*)world->stagingBufferMemory + WORLD_INDEX_BUFFER_SIZE);
			uint32_t* stagingColors = (uint32_t*)((uint8_t*)world->stagingBufferMemory + WORLD_INDEX_BUFFER_SIZE + WORLD_POSITION_BUFFER_SIZE);

			srand(1337);

			primitive_context_t ctx = {
				.indices = stagingIndices,
				.positions = stagingPositions,
				.colors = stagingColors,
			};

			for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i)
			{
				fill_primitive_data(&ctx, &world->layers[i].polygon, i);
			}

			//printf("World triangles: %u, vertices: %u\n", ctx.indexCount / 3u, ctx.vertexCount);

			world->indexCount = ctx.indexCount;
			
			PushStagingMemoryFlush(rc->stagingMemory, world->stagingBufferMemory, WORLD_STAGING_BUFFER_SIZE);
			
			{
				const VkBufferCopy copyRegion = {
					.size = WORLD_INDEX_BUFFER_SIZE,
				};
				vkCmdCopyBuffer(cb, world->stagingBuffer, world->indexBuffer, 1, &copyRegion);
			}
			{
				const VkBufferCopy copyRegion = {
					.size = WORLD_POSITION_BUFFER_SIZE,
					.srcOffset = WORLD_INDEX_BUFFER_SIZE,
				};
				vkCmdCopyBuffer(cb, world->stagingBuffer, world->vertexPositionBuffer, 1, &copyRegion);
			}
			{
				const VkBufferCopy copyRegion = {
					.size = WORLD_COLOR_BUFFER_SIZE,
					.srcOffset = WORLD_INDEX_BUFFER_SIZE + WORLD_POSITION_BUFFER_SIZE,
				};
				vkCmdCopyBuffer(cb, world->stagingBuffer, world->vertexColorBuffer, 1, &copyRegion);
			}

			//world->state = WORLD_STATE_DONE;
			break;
		}
		
		case WORLD_STATE_DONE:
		{
			break;
		}
	}
	
#if 0
	for (size_t i = 0; i < world->colliders.triangleCount; ++i)
	{
		triangle_collider_debug_draw(&world->colliders.triangles[i]);
	}
#endif

#if 0
	editor_polygon_debug_draw(&world->polygon);
#endif
}

int world_serialize(world_t* world, FILE* f)
{
	int r;

	uint polygonCount = PARALLAX_LAYER_COUNT;
	r = fwrite(&polygonCount, sizeof(uint), 1, f);
	assert(r == 1);
	
	for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i)
	{
		const editor_polygon_t* polygon = &world->layers[i].polygon;
		r = fwrite(&polygon->vertexCount, sizeof(uint), 1, f);
		assert(r == 1);
		r = fwrite(polygon->vertexPosition, sizeof(vec2), polygon->vertexCount, f);
		assert(r == polygon->vertexCount);
	}

	return 0;
}

int world_deserialize(world_t* world, FILE* f)
{
	int r;

	uint polygonCount;
	r = fread(&polygonCount, sizeof(uint), 1, f);
	assert(r == 1);
	assert(polygonCount == PARALLAX_LAYER_COUNT);
	
	for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i)
	{
		editor_polygon_t* polygon = &world->layers[i].polygon;
		r = fread(&polygon->vertexCount, sizeof(uint), 1, f);
		assert(r == 1);
		r = fread(polygon->vertexPosition, sizeof(vec2), polygon->vertexCount, f);
		assert(r == polygon->vertexCount);

		polygon->layer = i;
	}

	return 0;
}

bool world_get_render_info(world_render_info_t* info, world_t* world)
{
	// if (world->state != WORLD_STATE_DONE)
	// {
	// 	return false;
	// }

	info->indexBuffer			= world->indexBuffer;
	info->vertexPositionBuffer	= world->vertexPositionBuffer;
	info->vertexColorBuffer		= world->vertexColorBuffer;
	info->indexCount			= world->indexCount;

	return true;
}

void world_get_collision_info(world_collision_info_t* info, world_t* world)
{
	info->triangleCount	= world->colliders.triangleCount;
	info->triangles		= world->colliders.triangles;
	// info->polygonCount	= 1;
	// info->polygons		= &world->polygon;
}

static editor_polygon_t* g_editInfoPolygons[PARALLAX_LAYER_COUNT];

void world_get_edit_info(world_edit_info_t* info, world_t* world)
{
	for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i)
	{
		g_editInfoPolygons[i] = &world->layers[i].polygon;
	}

	info->polygonCount	= PARALLAX_LAYER_COUNT;
	info->polygons		= g_editInfoPolygons;
}

static void triangle_collider_debug_draw(triangle_collider_t* t)
{
	const debug_vertex_t v0 = { .x = t->a.x, .y = t->a.y, .color = 0xffffffff };
	const debug_vertex_t v1 = { .x = t->b.x, .y = t->b.y, .color = 0xffffffff };
	const debug_vertex_t v2 = { .x = t->c.x, .y = t->c.y, .color = 0xffffffff };
	
	DrawDebugLine(v0, v1);
	DrawDebugLine(v1, v2);
	DrawDebugLine(v2, v0);
}

float world_get_parallax_layer_depth(uint layerIndex)
{
	return (layerIndex / (float)PARALLAX_LAYER_COUNT) * -20.0f;
}

void editor_polygon_debug_draw(editor_polygon_t* p)
{
	const float depth = world_get_parallax_layer_depth(p->layer);

	for (size_t i = 0; i < p->vertexCount; ++i)
	{
		const size_t j = (i + 1) % p->vertexCount;

		const vec2 p0 = p->vertexPosition[i];
		const vec2 p1 = p->vertexPosition[j];

		const debug_vertex_t v0 = { .x = p0.x, .y = p0.y, .z = depth, .color = 0xffffffff };
		const debug_vertex_t v1 = { .x = p1.x, .y = p1.y, .z = depth, .color = 0xffffffff };
		DrawDebugLine(v0, v1);
	}

	for (size_t i = 0; i < p->vertexCount; ++i)
	{
		const size_t j = (i + 1) % p->vertexCount;
		const size_t k = (i + 2) % p->vertexCount;
		
		const vec2 p0 = p->vertexPosition[i];
		const vec2 p1 = p->vertexPosition[j];
		const vec2 p2 = p->vertexPosition[k];
		
		const vec2 d0 = vec2_normalize(vec2_sub(p1, p0));
		const vec2 d1 = vec2_normalize(vec2_sub(p2, p1));

		const float a0 = atan2f(d0.y, d0.x);
		const float a1 = atan2f(d1.y, d1.x);

		float da = a1 - a0;
		da = atan2f(sin(da), cos(da));

		const bool isConvex = da < 0.0f;
		
		DrawDebugPoint((debug_vertex_t){ .x = p1.x, .y = p1.y, .z = depth, .color = isConvex ? 0xff00ff00 : 0xff0000ff });
	}
}

// https://www.geometrictools.com/Documentation/TriangulationByEarClipping.pdf
static void editor_polygon_triangulate(triangle_t* triangles, size_t* triangleCount, const editor_polygon_t* p)
{
	*triangleCount = 0;

	uint workingSet[POLYGON_MAX_VERTICES];
	uint workingSetSize = p->vertexCount;

	for (size_t i = 0; i < p->vertexCount; ++i)
	{
		workingSet[i] = i;
	}

	while (workingSetSize > 3)
	{
		//printf("outer: %u\n", workingSetSize);

		for (size_t i = 0; i < workingSetSize && workingSetSize > 3; ++i)
		{
			//printf("inner: %u\n", i);

			const size_t j = (i + 1) % workingSetSize;
			const size_t k = (i + 2) % workingSetSize;
			
			const uint i0 = workingSet[i];
			const uint i1 = workingSet[j];
			const uint i2 = workingSet[k];
			
			const vec2 p0 = p->vertexPosition[i0];
			const vec2 p1 = p->vertexPosition[i1];
			const vec2 p2 = p->vertexPosition[i2];

			const vec2 d0 = vec2_normalize(vec2_sub(p1, p0));
			const vec2 d1 = vec2_normalize(vec2_sub(p2, p1));

			const float a0 = atan2f(d0.y, d0.x);
			const float a1 = atan2f(d1.y, d1.x);

			float da = a1 - a0;
			da = atan2f(sin(da), cos(da));

			const bool isConvex = da < 0.0f;

			if (!isConvex)
			{
				//printf("not convex\n");
				continue;
			}

			// verify clockwise winding
			const float det = 
				p0.x * (p1.y - p2.y) + 
				p1.x * (p2.y - p0.y) + 
				p2.x * (p0.y - p1.y);
			assert(det <= 0.0f);

			const vec2 edges[][2] = {
				{p0, p1},
				{p1, p2},
				{p2, p0},
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
			
			bool shouldClip = true;

			//printf("workingSetSize: %u\n", workingSetSize);
			uint insideTestCount = workingSetSize - 3;
			for (uint testIndex = 0; testIndex < insideTestCount; ++testIndex)
			{
				const uint vertexIndex = workingSet[(testIndex + k + 1) % workingSetSize];
				const vec2 pos = p->vertexPosition[vertexIndex];
				
				if (vec2_dot(vec3_xy(planes[0]), pos) - planes[0].z < 0.0f &&
					vec2_dot(vec3_xy(planes[1]), pos) - planes[1].z < 0.0f &&
					vec2_dot(vec3_xy(planes[2]), pos) - planes[2].z < 0.0f)
				{
					shouldClip = false;
					break;
				}
			}
			
			if (shouldClip)
			{
				triangle_t* triangle = &triangles[(*triangleCount)++];
				triangle->i[0] = i0;
				triangle->i[1] = i1;
				triangle->i[2] = i2;

				--workingSetSize;

				for (uint del = j; del < workingSetSize; ++del)
				{
					workingSet[del] = workingSet[del + 1];
				}
			}
		}
	}

	assert(workingSetSize == 3);

	triangle_t* triangle = &triangles[(*triangleCount)++];
	triangle->i[0] = workingSet[0];
	triangle->i[1] = workingSet[1];
	triangle->i[2] = workingSet[2];
}