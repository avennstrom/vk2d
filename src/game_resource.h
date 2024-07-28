#pragma once

#include <stdint.h>
#include <stdio.h>

#include "types.h"
#include "file_format.h"

typedef struct game_resource
{
	int										fd;
	const uint8_t*							mem;
	size_t									len;

	uint									modelCount;
	FILEFORMAT_game_resource_model_entry_t*	models;
} game_resource_t;

int game_resource_open(game_resource_t* gameResource);
void game_resource_close(game_resource_t* gameResource);