#include "glb_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define GLB_CHUNK_TYPE_JSON			0x4E4F534Au
#define GLB_CHUNK_TYPE_BIN			0x004E4942u

int glb_parse(glb_t* glb, const char* filename)
{
	*glb = (glb_t){0};

	FILE* f = fopen(filename, "rb");
	if (f == NULL)
	{
		return 1;
	}
	
	int r;
	glb_header_t header;
	glb_chunk_header_t chunk_header;

	r = fread(&header, sizeof(header), 1, f);
	assert(r == 1);
	
	if (header.magic != 0x46546C67u)
	{
		printf("error: invalid magic\n");
		return 1;
	}
	
	printf("glb version: %u\n", header.version);
	printf("glb length: %u\n", header.length);

	// JSON chunk
	{
		r = fread(&chunk_header, sizeof(chunk_header), 1, f);
		assert(r == 1);
		assert(chunk_header.chunkType == GLB_CHUNK_TYPE_JSON);

		printf("glb JSON chunk length: %u\n", chunk_header.chunkLength);

		char* json = malloc(chunk_header.chunkLength);
		r = fread(json, chunk_header.chunkLength, 1, f);
		assert(r == 1);

		//printf("%.*s", (int)chunk_header.chunkLength, json);
		gltf_parse(&glb->gltf, json, chunk_header.chunkLength);
	}
	
	// buffer chunk
	{
		r = fread(&chunk_header, sizeof(chunk_header), 1, f);
		assert(r == 1);
		assert(chunk_header.chunkType == GLB_CHUNK_TYPE_BIN);
		
		printf("glb buffer chunk length: %u\n", chunk_header.chunkLength);

		glb->buffer.len = chunk_header.chunkLength;
		glb->buffer.data = malloc(chunk_header.chunkLength);
		r = fread(glb->buffer.data, glb->buffer.len, 1, f);
		assert(r == 1);
	}
	
	fclose(f);

	return 0;
}