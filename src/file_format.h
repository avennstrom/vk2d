#pragma once

#include "types.h"

#include <stdint.h>

#define FILEFORMAT_model_VERSION 1

typedef struct FILEFORMAT_model_header
{
	uint32_t partCount;
} FILEFORMAT_model_header_t;

typedef struct FILEFORMAT_model_part_header
{
	vec4		rotation;
	vec3		translation;
	uint32_t	parentIndex;
	uint32_t	indexCount;
	uint32_t	vertexCount;
} FILEFORMAT_model_part_header_t;
