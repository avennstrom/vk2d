#include "editor.h"
#include "vec.h"

#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

typedef struct editor_camera
{
	vec2	pos;
	vec2	origin;
	float	height;
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
} editor_t;

static void calculate_camera(scb_camera_t* renderCamera, const editor_camera_t* camera, float aspectRatio);

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

void editor_window_event(editor_t* editor, const window_event_t* event)
{
	switch (event->type)
	{
		case WINDOW_EVENT_BUTTON_DOWN:
			if (event->data.button.button == BUTTON_LEFT)
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
			if (editor->mouseDragging)
			{
				editor->mouseDragTarget = mouseWorldPos;
			}
			if (editor->editPolygon != NULL)
			{
				editor->editPolygon->vertexPosition[editor->editVertex] = mouseWorldPos;
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
			if (event->data.scroll.delta > 0)
			{
				editor->camera.height *= 0.7f;
			}
			if (event->data.scroll.delta < 0)
			{
				editor->camera.height *= 1.3f;
			}
			break;
	}
}

void editor_render(scb_t* scb, editor_t* editor, uint2 resolution)
{
	editor->resolution = resolution;

	world_edit_info_t worldInfo;
	world_get_edit_info(&worldInfo, editor->world);

	{
		const vec2 worldMousePos = editor_project_screen_to_world(editor, editor->mousePos);
		DrawDebugPoint((debug_vertex_t){.x = worldMousePos.x, .y = worldMousePos.y, .color = 0xff00ffff});

		editor_polygon_t* polygon = worldInfo.polygons;

		editor->closestPolygon = NULL;
		
		if (editor->insertMode)
		{
			uint closestEdge = 0;
			float closestDistance = FLT_MAX;

			for (uint i = 0; i < polygon->vertexCount; ++i)
			{
				const uint j = (i + 1) % polygon->vertexCount;
				
				const vec2 a = polygon->vertexPosition[i];
				const vec2 b = polygon->vertexPosition[j];
				const vec2 tangent = vec2_normalize(vec2_sub(b, a));
				const vec2 normal = {-tangent.y, tangent.x};
				
				const vec2 center = vec2_scale(vec2_add(a, b), 0.5f);

				const float edgeLength = vec2_length(vec2_sub(b, a));
				const float tangentDistance = fabsf(vec2_dot(tangent, worldMousePos) - vec2_dot(tangent, center));
				if (tangentDistance > edgeLength * 0.5f)
				{
					continue;
				}
				
				const float normalDistance = fabsf(vec2_dot(normal, worldMousePos) - vec2_dot(normal, center));

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

				if (normalDistance < closestDistance)
				{
					closestDistance	= normalDistance;
					closestEdge		= i;
				}

				//printf("d: %f\n", d);
			}
			
			if (closestDistance < FLT_MAX)
			{
				const vec2 a = polygon->vertexPosition[closestEdge];
				const vec2 b = polygon->vertexPosition[(closestEdge + 1) % polygon->vertexCount];
				
				DrawDebugLine(
					(debug_vertex_t){.x = a.x, .y = a.y, .color = 0xff0000ff},
					(debug_vertex_t){.x = b.x, .y = b.y, .color = 0xff0000ff}
				);

				editor->closestPolygon	= polygon;
				editor->closestVertex	= closestEdge;
			}
		}
		else
		{
			uint closestVertex = 0;
			float closestDistance = FLT_MAX;

			for (uint i = 0; i < polygon->vertexCount; ++i)
			{
				const vec2 p = polygon->vertexPosition[i];
				const float d = vec2_length(vec2_sub(p, worldMousePos));
				if (d < closestDistance)
				{
					closestDistance	= d;
					closestVertex	= i;
				}
			}
			
			editor->closestPolygon	= polygon;
			editor->closestVertex	= closestVertex;
			
			const vec2 closestPos = polygon->vertexPosition[closestVertex];
			DrawDebugPoint((debug_vertex_t){.x = closestPos.x, .y = closestPos.y, .color = 0xffffff00});
		}
	}

	if (editor->mouseDragging)
	{
		const vec2 dragDelta = vec2_sub(editor->mouseDragOrigin, editor->mouseDragTarget);
		editor->camera.pos = vec2_add(editor->camera.origin, dragDelta);
	}
	
	DrawDebugPoint((debug_vertex_t){.x = editor->mouseDragOrigin.x, .y = editor->mouseDragOrigin.y, .color = 0xff0000ff});

	const float aspectRatio = resolution.x / (float)resolution.y;
	calculate_camera(scb_set_camera(scb), &editor->camera, aspectRatio);
}

static void calculate_camera(scb_camera_t* renderCamera, const editor_camera_t* camera, float aspectRatio)
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