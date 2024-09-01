#include "editor.h"
#include "vec.h"
#include "intersection.h"

#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <assert.h>

typedef struct editor_camera
{
	vec2	pos;
	vec2	origin;
	float	height;
	float	zoom;

	mat4	transform;
	mat4	viewMatrix;
	mat4	projectionMatrix;
	mat4	viewProjectionMatrix;
} editor_camera_t;

typedef struct editor
{
	world_t*		world;
	uint2			resolution;
	editor_camera_t	camera;
	bool			mouseDragging;
	vec2			mouseDragOrigin;
	vec2			mouseDragTarget;
	int2			mousePos;
	
	editor_polygon_t*	closestPolygon;
	uint				closestVertex;

	editor_polygon_t*	editPolygon;
	uint				editVertex;

	bool				insertMode;

	int					firstVisibleLayer;
} editor_t;

static void update_camera_matrices(editor_camera_t* camera, float aspectRatio);
static void calculate_camera(scb_camera_t* renderCamera, const editor_camera_t* camera);

editor_t* editor_create(world_t* world)
{
	editor_t* editor = calloc(1, sizeof(editor_t));
	if (editor == NULL)
	{
		return NULL;
	}
	
	editor->world = world;

	editor->camera = (editor_camera_t){
		.height = 10.0f,
	};

	return editor;
}

void editor_destroy(editor_t* editor)
{
	free(editor);
}

static vec2 editor_project_screen_to_world(const editor_t* editor, int2 screen)
{
	const editor_camera_t* camera = &editor->camera;
	const float aspectRatio = editor->resolution.x / (float)editor->resolution.y;

	const vec2 uv = {
		screen.x / (float)editor->resolution.x,
		1.0f - screen.y / (float)editor->resolution.y,
	};
	
	const vec2 ndc = vec2_sub(uv, (vec2){0.5f, 0.5f});
	const vec2 ws = vec2_mul(ndc, (vec2){camera->height * aspectRatio, camera->height});
	return vec2_add(ws, camera->origin);
}

static vec2 editor_project_world_to_screen(const editor_camera_t* camera, vec3 p)
{
	vec4 pp = mat_mul_vec4(camera->viewProjectionMatrix, (vec4){p.x, p.y, p.z, 1.0f});
	pp.x /= pp.w;
	pp.y /= pp.w;
	pp.z /= pp.w;
	pp.x *= 0.5f;
	pp.y *= -0.5f;
	pp.x += 0.5f;
	pp.y += 0.5f;
	return (vec2){pp.x, pp.y};
}

static void editor_create_camera_ray(vec3* origin, vec3* direction, const editor_camera_t* camera, vec2 uv)
{
	const mat4 inverseViewProjectionMatrix = mat_invert(camera->viewProjectionMatrix);

	vec3 near;
	vec3 far;

	vec3 ndc;
	ndc.x = uv.x * 2.0f - 1.0f;
	ndc.y = uv.y * -2.0f + 1.0f;

	ndc.z = 0.0f;
	near = mat_mul_hom(inverseViewProjectionMatrix, (vec4){ndc.x, ndc.y, ndc.z, 1.0f});
	
	ndc.z = 1.0f;
	far = mat_mul_hom(inverseViewProjectionMatrix, (vec4){ndc.x, ndc.y, ndc.z, 1.0f});

	*direction = vec3_normalize(vec3_sub(far, near));
	*origin = near;
}

void editor_window_event(editor_t* editor, const window_event_t* event)
{
	switch (event->type)
	{
		case WINDOW_EVENT_BUTTON_DOWN:
			if (event->data.button.button == BUTTON_LEFT)
			{
				if (editor->closestPolygon != NULL)
				{
					if (editor->insertMode)
					{
						for (int i = editor->closestPolygon->vertexCount - 1; i > editor->closestVertex; --i)
						{
							editor->closestPolygon->vertexPosition[i + 1] = editor->closestPolygon->vertexPosition[i];
						}
						++editor->closestPolygon->vertexCount;
						editor->closestPolygon->vertexPosition[editor->closestVertex + 1] = editor_project_screen_to_world(editor, event->data.button.pos);

						editor->editPolygon	= editor->closestPolygon;
						editor->editVertex	= editor->closestVertex + 1;
					}
					else
					{
						editor->editPolygon	= editor->closestPolygon;
						editor->editVertex	= editor->closestVertex;
					}
				}
			}
			if (event->data.button.button == BUTTON_RIGHT)
			{
				editor->mouseDragging = true;
				editor->mouseDragOrigin = editor_project_screen_to_world(editor, event->data.button.pos);
				editor->mouseDragTarget = editor->mouseDragOrigin;
			}
			break;
		case WINDOW_EVENT_BUTTON_UP:
			if (event->data.button.button == BUTTON_LEFT)
			{
				editor->editPolygon	= NULL;
				editor->editVertex	= 0;
			}
			if (event->data.button.button == BUTTON_RIGHT)
			{
				editor->mouseDragging = false;

				const vec2 dragDelta = vec2_sub(editor->mouseDragOrigin, editor->mouseDragTarget);
				editor->camera.origin = vec2_add(editor->camera.origin, dragDelta);
				editor->camera.pos = editor->camera.origin;
			}
			break;
		case WINDOW_EVENT_MOUSE_MOVE:
			editor->mousePos = event->data.mouse.pos;
			const vec2 mouseWorldPos = editor_project_screen_to_world(editor, event->data.mouse.pos);
			const vec2 mouseUv = { editor->mousePos.x / (float)editor->resolution.x, editor->mousePos.y / (float)editor->resolution.y };
			if (editor->mouseDragging)
			{
				editor->mouseDragTarget = mouseWorldPos;
			}
			if (editor->editPolygon != NULL)
			{
				float depth = world_get_parallax_layer_depth(editor->editPolygon->layer);

				vec3 origin;
				vec3 direction;
				editor_create_camera_ray(&origin, &direction, &editor->camera, mouseUv);
				float hitDistance;
				bool hit = intersect_ray_plane(&hitDistance, origin, direction, (vec4){0.0f, 0.0f, 1.0f, depth});
				assert(hit);
				const vec3 hitPosition = vec3_add(origin, vec3_scale(direction, hitDistance));
				editor->editPolygon->vertexPosition[editor->editVertex] = vec3_xy(hitPosition);
			}
			break;
		case WINDOW_EVENT_KEY_DOWN:
			if (event->data.key.code == KEY_CONTROL)
			{
				editor->insertMode = true;
			}
			if (event->data.key.code == KEY_DELETE)
			{
				if (editor->editPolygon != NULL && editor->editPolygon->vertexCount > 3)
				{
					--editor->editPolygon->vertexCount;
					for (uint i = editor->editVertex; i < editor->editPolygon->vertexCount; ++i)
					{
						editor->editPolygon->vertexPosition[i] = editor->editPolygon->vertexPosition[i + 1];
					}

					editor->editPolygon = NULL;
				}
			}
			break;
		case WINDOW_EVENT_KEY_UP:
			if (event->data.key.code == KEY_CONTROL)
			{
				editor->insertMode = false;
			}
			break;
		case WINDOW_EVENT_MOUSE_SCROLL:
			if (editor->insertMode)
			{
				editor->firstVisibleLayer += event->data.scroll.delta;
				if (editor->firstVisibleLayer >= PARALLAX_LAYER_COUNT)
				{
					editor->firstVisibleLayer = PARALLAX_LAYER_COUNT - 1;
				}
				if (editor->firstVisibleLayer < 0)
				{
					editor->firstVisibleLayer = 0;
				}
			}
			else
			{
				if (event->data.scroll.delta > 0)
				{
					editor->camera.zoom -= 1.0f;
				}
				if (event->data.scroll.delta < 0)
				{
					editor->camera.zoom += 1.0f;
				}
			}
			break;
	}
}

void editor_render(scb_t* scb, editor_t* editor, uint2 resolution)
{
	editor->resolution = resolution;

	const float aspectRatio = resolution.x / (float)resolution.y;
	update_camera_matrices(&editor->camera, aspectRatio);

	world_edit_info_t worldInfo;
	world_get_edit_info(&worldInfo, editor->world);
	
	uint32_t visibleLayerMask = 0xffffffffu;
	visibleLayerMask &= ~((1 << editor->firstVisibleLayer) - 1);
	world_set_visible_layers(editor->world, visibleLayerMask);

	editor->closestPolygon = NULL;
	float closestDistance = FLT_MAX;

	for (int polygonIndex = editor->firstVisibleLayer; polygonIndex < worldInfo.polygonCount; ++polygonIndex)
	{
		editor_polygon_t* polygon = worldInfo.polygons[polygonIndex];
		const float polygonDepth = world_get_parallax_layer_depth(polygon->layer);
		
		const vec2 mouseUv = { editor->mousePos.x / (float)resolution.x, editor->mousePos.y / (float)resolution.y };
		const editor_camera_t* camera = &editor->camera;

		triangle_t triangles[256];
		size_t triangleCount;
		editor_polygon_triangulate(triangles, &triangleCount, polygon);

		bool isMouseInsideTriangle = false;

		for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
		{
			const triangle_t* triangle = &triangles[triangleIndex];
			const vec2 v0 = polygon->vertexPosition[triangle->i[0]];
			const vec2 v1 = polygon->vertexPosition[triangle->i[1]];
			const vec2 v2 = polygon->vertexPosition[triangle->i[2]];
			const vec2 pv0 = editor_project_world_to_screen(camera, (vec3){v0.x, v0.y, polygonDepth});
			const vec2 pv1 = editor_project_world_to_screen(camera, (vec3){v1.x, v1.y, polygonDepth});
			const vec2 pv2 = editor_project_world_to_screen(camera, (vec3){v2.x, v2.y, polygonDepth});
			
			if (intersect_point_triangle_2d(mouseUv, pv0, pv1, pv2))
			{
				isMouseInsideTriangle = true;
				break;
			}
		}

		if (!isMouseInsideTriangle)
		{
			continue;
		}
		
		if (editor->insertMode)
		{
#if 1
			uint closestEdge = 0;
			float closestEdgeDistance = FLT_MAX;

			for (uint i = 0; i < polygon->vertexCount; ++i)
			{
				const uint j = (i + 1) % polygon->vertexCount;
				
				const vec2 a = editor_project_world_to_screen(camera, (vec3){polygon->vertexPosition[i].x, polygon->vertexPosition[i].y, polygonDepth});
				const vec2 b = editor_project_world_to_screen(camera, (vec3){polygon->vertexPosition[j].x, polygon->vertexPosition[j].y, polygonDepth});
				const vec2 tangent = vec2_normalize(vec2_sub(b, a));
				const vec2 normal = {-tangent.y, tangent.x};
				
				const vec2 center = vec2_scale(vec2_add(a, b), 0.5f);

				const float edgeLength = vec2_length(vec2_sub(b, a));
				const float tangentDistance = fabsf(vec2_dot(tangent, mouseUv) - vec2_dot(tangent, center));
				if (tangentDistance > edgeLength * 0.5f)
				{
					continue;
				}
				
				const float normalDistance = fabsf(vec2_dot(normal, mouseUv) - vec2_dot(normal, center));

#if 0
				DrawDebugLine(
					(debug_vertex_t){.x = center.x, .y = center.y, .color = 0xffffffff},
					(debug_vertex_t){.x = center.x - tangent.x * tangentDistance, .y = center.y - tangent.y * tangentDistance, .color = 0xffffffff}
				);
				DrawDebugLine(
					(debug_vertex_t){.x = center.x, .y = center.y, .color = 0xff00ff00},
					(debug_vertex_t){.x = center.x - normal.x * normalDistance, .y = center.y - normal.y * normalDistance, .color = 0xff00ff00}
				);
#endif

				if (normalDistance < closestEdgeDistance)
				{
					closestEdgeDistance	= normalDistance;
					closestEdge			= i;
				}

				//printf("d: %f\n", d);
			}

			if (closestEdgeDistance < closestDistance)
			{
				closestDistance			= closestEdgeDistance;
				editor->closestPolygon	= polygon;
				editor->closestVertex	= closestEdge;
			}
#endif
		}
		else
		{
			uint closestVertex = 0;
			float closestVertexDistance = FLT_MAX;

			for (uint i = 0; i < polygon->vertexCount; ++i)
			{
				const vec2 p2 = polygon->vertexPosition[i];
				//const float depth = world_get_parallax_layer_depth(polygon->layer);
				
				const vec2 pp = editor_project_world_to_screen(camera, (vec3){p2.x, p2.y, polygonDepth});
				//DrawDebugPoint2D((debug_vertex_t){.x = pp.x, .y = pp.y, .color = 0xffff00ff});

				//printf("%.1f, %.1f\n", pp.x, pp.y);

				const float d = vec2_distance((vec2){pp.x, pp.y}, mouseUv);
				if (d < closestVertexDistance)
				{
					closestVertexDistance	= d;
					closestVertex			= i;
				}
			}

			if (closestVertexDistance < closestDistance)
			{
				closestDistance			= closestVertexDistance;
				editor->closestPolygon	= polygon;
				editor->closestVertex	= closestVertex;
			}
		}
		
		if (isMouseInsideTriangle)
		{
			break;
		}
	}

	if (editor->closestPolygon != NULL)
	{
		editor_polygon_t* polygon = editor->closestPolygon;
		const float depth = world_get_parallax_layer_depth(polygon->layer);
		
		if (editor->insertMode)
		{
			const vec2 a = polygon->vertexPosition[editor->closestVertex];
			const vec2 b = polygon->vertexPosition[(editor->closestVertex + 1) % polygon->vertexCount];
			
			DrawDebugLine(
				(debug_vertex_t){.x = a.x, .y = a.y, .z = depth, .color = 0xff0000ff},
				(debug_vertex_t){.x = b.x, .y = b.y, .z = depth, .color = 0xff0000ff}
			);
		}
		else
		{
			const vec2 closestPos = polygon->vertexPosition[editor->closestVertex];
			const vec2 pp = editor_project_world_to_screen(&editor->camera, (vec3){closestPos.x, closestPos.y, depth});
			DrawDebugPoint2D((debug_vertex_t){.x = pp.x, .y = pp.y, .color = 0xffffff00});
			
			editor_polygon_debug_draw(polygon);
		}
	}

	if (editor->mouseDragging)
	{
		const vec2 dragDelta = vec2_sub(editor->mouseDragOrigin, editor->mouseDragTarget);
		editor->camera.pos = vec2_add(editor->camera.origin, dragDelta);
	}
	
	DrawDebugPoint((debug_vertex_t){.x = editor->mouseDragOrigin.x, .y = editor->mouseDragOrigin.y, .color = 0xff0000ff});

	calculate_camera(scb_set_camera(scb), &editor->camera);
}

static void update_camera_matrices(editor_camera_t* camera, float aspectRatio)
{
	float zoomOffset = camera->zoom;
	float fovOffset = camera->zoom * 2.0f;
	
	if (zoomOffset < 0.0f)
	{
		zoomOffset = 0.0f;
	}

	if (fovOffset > 0.0f)
	{
		fovOffset = 0.0f;
	}

	zoomOffset = powf(zoomOffset, 1.5f);

	mat4 m = mat_identity();
	m = mat_translate(m, (vec3){ camera->pos.x, camera->pos.y, CAMERA_OFFSET + zoomOffset });

	//const vec2 viewSize = {camera->height * aspectRatio, camera->height};

	camera->transform = m;
	camera->viewMatrix = mat_invert(m);
	//const mat4 projectionMatrix = mat_orthographic(viewSize, 0.0f, 1.0f);
	camera->projectionMatrix = mat_perspective(CAMERA_FOV + fovOffset, aspectRatio, CAMERA_NEAR, CAMERA_FAR);
	camera->viewProjectionMatrix = mat_mul(camera->viewMatrix, camera->projectionMatrix);
}

static void calculate_camera(scb_camera_t* renderCamera, const editor_camera_t* camera)
{
	// :TODO: transpose these in the scene instead
	renderCamera->transform = mat_transpose(camera->transform);
	renderCamera->viewMatrix = mat_transpose(camera->viewMatrix);
	renderCamera->projectionMatrix = mat_transpose(camera->projectionMatrix);
	renderCamera->viewProjectionMatrix = mat_transpose(camera->viewProjectionMatrix);
}