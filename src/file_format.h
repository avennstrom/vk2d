#pragma once

#include "types.h"

#include <stdint.h>

#define FILEFORMAT_game_resource_VERSION 1

typedef struct FILEFORMAT_model_header
{
	uint64_t	contentOffset;
	uint32_t	partCount;
	uint32_t	dataSize;
} FILEFORMAT_model_header_t;

typedef struct FILEFORMAT_model_part_header
{
	vec4		rotation;
	vec3		translation;
	uint32_t	parentIndex;
	uint32_t	indexCount;
	uint32_t	vertexCount;

	// byte offsets
	uint32_t	indexDataOffset;
	uint32_t	vertexPositionDataOffset;
	uint32_t	vertexNormalDataOffset;
	uint32_t	vertexColorDataOffset;
} FILEFORMAT_model_part_header_t;

typedef struct FILEFORMAT_game_resource_header
{
	uint32_t	version;
	uint32_t	modelCount;
} FILEFORMAT_game_resource_header_t;

typedef struct FILEFORMAT_game_resource_model_entry
{
	uint64_t	headerOffset;
} FILEFORMAT_game_resource_model_entry_t;