#pragma once

#include "world.h"
#include "scene.h"
#include "window.h"

typedef struct editor editor_t;

editor_t* editor_create(world_t* world);
void editor_destroy(editor_t* editor);

void editor_window_event(editor_t* editor, const window_event_t* event);
void editor_render(scb_t* scb, editor_t* editor, uint2 resolution);