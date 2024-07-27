#pragma once

#include "types.h"

#include <stdint.h>

#define FILEFORMAT_model_VERSION 2

typedef struct FILEFORMAT_model_header
{
	uint32_t partCount;
	uint32_t dataSize;
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

typedef struct FILEFORMAT_resource_manifest
{
	uint32_t	hierarchySize;
} FILEFORMAT_resource_manifest_t;