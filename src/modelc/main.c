#include "glb_parser.h"
#include "../file_format.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void print_usage(const char* argv0)
{
	printf("\n", argv0);
	printf("Usage:\n", argv0);
	printf("%s -o output_file.model -i input_file.glb\n", argv0);
}

static void export_model(FILE* f, const glb_t* glb, uint32_t meshIndex)
{
	const gltf_t* gltf = &glb->gltf;

	const gltf_mesh_t* mesh = &gltf->meshes[meshIndex];

	assert(mesh->primitiveCount == 1); // :todo:
	const gltf_primitive_t* prim = &mesh->primitives[0];
	
	const gltf_accessor_t* indexAccessor = &gltf->accessors[prim->indices];
	assert(indexAccessor->componentType == GLTF_ACCESSOR_COMPONENT_TYPE_UINT16);
	assert(indexAccessor->type == GLTF_ACCESSOR_TYPE_SCALAR);

	const gltf_buffer_view_t* indexView = &gltf->bufferViews[indexAccessor->bufferView];
	assert(indexView->buffer == 0);

	assert(prim->attributes[GLTF_ATTRIBUTE_POSITION] != 0xffffffff);
	const gltf_accessor_t* positionAccessor = &gltf->accessors[prim->attributes[GLTF_ATTRIBUTE_POSITION]];
	assert(positionAccessor->componentType == GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT);
	assert(positionAccessor->type == GLTF_ACCESSOR_TYPE_VEC3);

	const gltf_buffer_view_t* positionView = &gltf->bufferViews[positionAccessor->bufferView];
	assert(positionView->buffer == 0);

	assert(prim->attributes[GLTF_ATTRIBUTE_NORMAL] != 0xffffffff);
	const gltf_accessor_t* normalAccessor = &gltf->accessors[prim->attributes[GLTF_ATTRIBUTE_NORMAL]];
	assert(normalAccessor->componentType == GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT);
	assert(normalAccessor->type == GLTF_ACCESSOR_TYPE_VEC3);

	const gltf_buffer_view_t* normalView = &gltf->bufferViews[normalAccessor->bufferView];
	assert(normalView->buffer == 0);

	assert(positionAccessor->count == normalAccessor->count);

	const FILEFORMAT_model_header_t header = {
		.version = FILEFORMAT_model_VERSION,
		.indexCount = indexAccessor->count,
		.vertexCount = positionAccessor->count,
	};

	int r;
	
	r = fwrite(&header, sizeof(header), 1, f);
	assert(r == 1);

	r = fwrite(glb->buffer.data + indexView->byteOffset, indexView->byteLength, 1, f);
	assert(r == 1);

	r = fwrite(glb->buffer.data + positionView->byteOffset, positionView->byteLength, 1, f);
	assert(r == 1);

	r = fwrite(glb->buffer.data + normalView->byteOffset, normalView->byteLength, 1, f);
	assert(r == 1);
}

int main(int argc, char** argv)
{	
	const char* inPath = NULL;
	const char* outPath = NULL;

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "-i") == 0)
		{
			if (i + 1 == argc)
			{
				printf("error: missing argument after -i\n");
				return 1;
			}
			inPath = argv[++i];
		}
		else if (strcmp(argv[i], "-o") == 0)
		{
			if (i + 1 == argc)
			{
				printf("error: missing argument after -i\n");
				return 1;
			}
			outPath = argv[++i];
		}
	}

	int argerror = 0;
	
	if (inPath == NULL)
	{
		printf("error: missing command line argument -i\n");
		argerror = 1;
	}
	if (outPath == NULL)
	{
		printf("error: missing command line argument -o\n");
		argerror = 1;
	}

	if (argerror != 0)
	{
		print_usage(argv[0]);
		return 1;
	}

	printf("%s -> %s\n", inPath, outPath);

	glb_t glb;
	glb_parse(&glb, inPath);

	gltf_dump(&glb.gltf);

	FILE* outFile = fopen(outPath, "wb");
	if (outFile == NULL)
	{
		printf("error: failed to open output file '%s'\n", outPath);
		return 1;
	}

	export_model(outFile, &glb, 0);

	fclose(outFile);

	return 0;
}