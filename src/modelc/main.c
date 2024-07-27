#include "glb_parser.h"
#include "../file_format.h"
#include "../types.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void print_usage(const char* argv0)
{
	printf("\n", argv0);
	printf("Usage:\n", argv0);
	printf("%s -o output_file.model -i input_file.glb -r root_node_name\n", argv0);
}

typedef struct model_hierarchy_node
{
	const gltf_node_t*	gltfNode;
	uint32_t			parentIndex;
} model_hierarchy_node_t;

typedef struct model_part
{
	uint						indexCount;
	uint						vertexCount;
	const gltf_buffer_view_t*	indexBufferView;
	const gltf_buffer_view_t*	positionBufferView;
	const gltf_buffer_view_t*	normalBufferView;
	const uint32_t*				vertexColor;
} model_part_t;

static void pack_vertex_color_from_uint16(uint32_t* target, const uint16_t* src, size_t vertexCount)
{
	for (size_t i = 0; i < vertexCount; ++i)
	{
		// const uint8_t r = (uint8_t)(src[0] >> 8);
		// const uint8_t g = (uint8_t)(src[1] >> 8);
		// const uint8_t b = (uint8_t)(src[2] >> 8);
		// const uint8_t a = (uint8_t)(src[3] >> 8);
		const uint8_t r = (uint8_t)((src[0] / (float)0xffff) * (float)0xff);
		const uint8_t g = (uint8_t)((src[1] / (float)0xffff) * (float)0xff);
		const uint8_t b = (uint8_t)((src[2] / (float)0xffff) * (float)0xff);
		//const uint8_t a = (uint8_t)((src[3] / (float)0xffff) * (float)0xff);
		
		target[0] = r | (g << 8) | (b << 16);

		target += 1;
		src += 4;
	}
}

static int export_model(FILE* f, const glb_t* glb, const char* rootNodeName)
{
	const gltf_t* gltf = &glb->gltf;

	uint32_t rootNodeIndex = 0xffffffff;
	for (size_t i = 0; i < gltf->nodeCount; ++i)
	{
		if (strcmp(gltf->nodes[i].name, rootNodeName) == 0)
		{
			rootNodeIndex = i;
			break;
		}
	}

	if (rootNodeIndex == 0xffffffff)
	{
		fprintf(stderr, "error: root node node '%s' not found\n", rootNodeName);
		return 1;
	}

	size_t hierarchySize = 0;
	model_hierarchy_node_t hierarchy[GLTF_PARSER_MAX_NODES];
	model_part_t parts[GLTF_PARSER_MAX_NODES];
	
	size_t sp = 0;
	uint2 stack[GLTF_PARSER_MAX_NODES];

	stack[sp++] = (uint2){rootNodeIndex, 0xffffffff};

	while (sp > 0)
	{
		const uint2 top = stack[--sp];
		const uint gltfNodeIndex = top.x;

		const gltf_node_t* gltfNode = &gltf->nodes[gltfNodeIndex];
		for (size_t i = 0; i < gltfNode->childCount; ++i)
		{
			const uint childNodeIndex = gltfNode->children[i];
			stack[sp++] = (uint2){ childNodeIndex, hierarchySize };
		}

		hierarchy[hierarchySize++] = (model_hierarchy_node_t){
			.gltfNode		= gltfNode,
			.parentIndex	= top.y,
		};
	}

	for (size_t i = 0; i < hierarchySize; ++i)
	{
		printf("hierarchy[%u].name: %s\n", i, hierarchy[i].gltfNode->name);
		printf("hierarchy[%u].parentIndex: %u\n", i, hierarchy[i].parentIndex);
	}

	for (size_t i = 0; i < hierarchySize; ++i)
	{
		const gltf_node_t* node = hierarchy[i].gltfNode;
		assert(node->type == GLTF_NODE_TYPE_MESH);

		const gltf_mesh_t* mesh = &gltf->meshes[node->mesh];
		//printf("[%u] mesh name: %s\n", i, mesh->name);

		assert(mesh->primitiveCount == 1); // todo?
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

		const uint32_t vertexCount = positionAccessor->count;

		const gltf_buffer_view_t* positionView = &gltf->bufferViews[positionAccessor->bufferView];
		assert(positionView->buffer == 0);

		assert(prim->attributes[GLTF_ATTRIBUTE_NORMAL] != 0xffffffff);
		const gltf_accessor_t* normalAccessor = &gltf->accessors[prim->attributes[GLTF_ATTRIBUTE_NORMAL]];
		assert(normalAccessor->componentType == GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT);
		assert(normalAccessor->type == GLTF_ACCESSOR_TYPE_VEC3);
		const gltf_buffer_view_t* normalView = &gltf->bufferViews[normalAccessor->bufferView];
		assert(normalView->buffer == 0);

		assert(prim->attributes[GLTF_ATTRIBUTE_COLOR] != 0xffffffff);
		const gltf_accessor_t* colorAccessor = &gltf->accessors[prim->attributes[GLTF_ATTRIBUTE_COLOR]];
		assert(colorAccessor->componentType == GLTF_ACCESSOR_COMPONENT_TYPE_UINT16);
		assert(colorAccessor->type == GLTF_ACCESSOR_TYPE_VEC4);
		const gltf_buffer_view_t* colorView = &gltf->bufferViews[colorAccessor->bufferView];
		assert(colorView->buffer == 0);

		assert(positionAccessor->count == vertexCount);
		assert(normalAccessor->count == vertexCount);
		assert(colorAccessor->count == vertexCount);

		uint32_t* packedVertexColor = malloc(vertexCount * sizeof(uint32_t));
		pack_vertex_color_from_uint16(packedVertexColor, (uint16_t*)(glb->buffer.data + colorView->byteOffset), vertexCount);

		parts[i] = (model_part_t){
			.indexCount = indexAccessor->count,
			.vertexCount = positionAccessor->count,
			.indexBufferView = indexView,
			.positionBufferView = positionView,
			.normalBufferView = normalView,
			.vertexColor = packedVertexColor,
		};
	}

	FILEFORMAT_model_part_header_t partHeaders[64];
	uint32_t dataOffset = 0u;

	for (int i = 0; i < hierarchySize; ++i)
	{
		const model_part_t* part = &parts[i];
		const model_hierarchy_node_t* hierarchyNode = &hierarchy[i];
		const gltf_node_t* gltfNode = hierarchyNode->gltfNode;

		FILEFORMAT_model_part_header_t partHeader = {
			.rotation		= (vec4){ gltfNode->rotation[0], gltfNode->rotation[1], gltfNode->rotation[2], gltfNode->rotation[3] },
			.translation	= (vec3){ gltfNode->translation[0], gltfNode->translation[1], gltfNode->translation[2] },
			.parentIndex	= hierarchy[i].parentIndex,
			.indexCount		= part->indexCount,
			.vertexCount	= part->vertexCount,
		};

		assert(dataOffset % 2 == 0);
		partHeader.indexDataOffset = dataOffset;
		dataOffset += part->indexCount * sizeof(uint16_t);

		assert(dataOffset % 4 == 0); // for storage buffer loads
		partHeader.vertexPositionDataOffset = dataOffset;
		dataOffset += part->vertexCount * sizeof(vec3);

		partHeader.vertexNormalDataOffset = dataOffset;
		dataOffset += part->vertexCount * sizeof(vec3);
		
		partHeader.vertexColorDataOffset = dataOffset;
		dataOffset += part->vertexCount * sizeof(uint32_t);

		// printf("indexDataOffset: %u\n", partHeader.indexDataOffset);
		// printf("vertexPositionDataOffset: %u\n", partHeader.vertexPositionDataOffset);
		// printf("vertexNormalDataOffset: %u\n", partHeader.vertexNormalDataOffset);
		// printf("vertexColorDataOffset: %u\n", partHeader.vertexColorDataOffset);

		partHeaders[i] = partHeader;
	}

	const FILEFORMAT_model_header_t header = {
		.partCount = hierarchySize,
		.dataSize = dataOffset,
	};

	int r;

	int fileVersion = FILEFORMAT_model_VERSION;
	r = fwrite(&fileVersion, sizeof(fileVersion), 1, f);
	
	r = fwrite(&header, sizeof(header), 1, f);
	assert(r == 1);

	r = fwrite(partHeaders, sizeof(partHeaders[0]), hierarchySize, f);
	assert(r == hierarchySize);

	for (int i = 0; i < hierarchySize; ++i)
	{
		const model_part_t* part = &parts[i];

		r = fwrite(glb->buffer.data + part->indexBufferView->byteOffset, part->indexBufferView->byteLength, 1, f);
		assert(r == 1);

		r = fwrite(glb->buffer.data + part->positionBufferView->byteOffset, part->positionBufferView->byteLength, 1, f);
		assert(r == 1);

		r = fwrite(glb->buffer.data + part->normalBufferView->byteOffset, part->normalBufferView->byteLength, 1, f);
		assert(r == 1);

		r = fwrite(part->vertexColor, part->vertexCount * sizeof(uint32_t), 1, f);
		assert(r == 1);
	}

	return 0;
}

int main(int argc, char** argv)
{	
	const char* inPath = NULL;
	const char* outPath = NULL;
	const char* rootNodeName = NULL;

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
		else if (strcmp(argv[i], "-r") == 0)
		{
			if (i + 1 == argc)
			{
				printf("error: missing argument after -r\n");
				return 1;
			}
			rootNodeName = argv[++i];
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
	if (rootNodeName == NULL)
	{
		printf("error: missing command line argument -r\n");
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

	//gltf_dump(&glb.gltf);

	FILE* outFile = fopen(outPath, "wb");
	if (outFile == NULL)
	{
		printf("error: failed to open output file '%s'\n", outPath);
		return 1;
	}

	if (export_model(outFile, &glb, rootNodeName) != 0)
	{
		printf("error: failed to export model\n");
		return 1;
	}

	fclose(outFile);

	return 0;
}