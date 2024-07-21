#pragma once

#include "gltf_parser.h"

#include <stdint.h>
#include <stddef.h>

// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#glb-file-format-specification
typedef struct glb_header
{
	uint32_t magic;
	uint32_t version;
	uint32_t length;
} glb_header_t;

typedef struct glb_chunk_header
{
	uint32_t chunkLength;
	uint32_t chunkType;
} glb_chunk_header_t;

_Static_assert(sizeof(glb_header_t) == 12, "");
_Static_assert(sizeof(glb_chunk_header_t) == 8, "");

typedef struct glb_buffer
{
	uint8_t*	data;
	size_t		len;
} glb_buffer_t;

typedef struct glb
{
	gltf_t			gltf;
	glb_buffer_t	buffer;
} glb_t;

int glb_parse(glb_t* glb, const char* filename);