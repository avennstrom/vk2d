#include "glb_parser.h"
#include "../file_format.h"
#include "../types.h"
#include "../util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sys/stat.h>

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

typedef struct model_hierarchy_node
{
	const gltf_node_t*	gltfNode;
	uint				parentIndex;
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

typedef struct extracted_model
{
	uint					partCount;
	model_part_t			parts[GLTF_PARSER_MAX_NODES];
	model_hierarchy_node_t	hierarchy[GLTF_PARSER_MAX_NODES];
} extracted_model_t;

static int extract_model(extracted_model_t* extracted, const glb_t* glb, const char* rootNodeName)
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
	//model_hierarchy_node_t hierarchy[GLTF_PARSER_MAX_NODES];
	
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

		extracted->hierarchy[hierarchySize++] = (model_hierarchy_node_t){
			.gltfNode		= gltfNode,
			.parentIndex	= top.y,
		};
	}

	const size_t partCount = hierarchySize;

#if 0
	for (size_t i = 0; i < partCount; ++i)
	{
		printf("hierarchy[%zu].name: %s\n", i, extracted->hierarchy[i].gltfNode->name);
		printf("hierarchy[%zu].parentIndex: %u\n", i, extracted->hierarchy[i].parentIndex);
	}
#endif

	for (size_t i = 0; i < partCount; ++i)
	{
		const gltf_node_t* node = extracted->hierarchy[i].gltfNode;
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

		extracted->parts[i] = (model_part_t){
			.indexCount = indexAccessor->count,
			.vertexCount = positionAccessor->count,
			.indexBufferView = indexView,
			.positionBufferView = positionView,
			.normalBufferView = normalView,
			.vertexColor = packedVertexColor,
		};
	}

	extracted->partCount = partCount;

	return 0;
}

typedef struct model_convert_action
{
	const char* sourcePath;
	const char* rootNodeName;
} model_convert_action_t;

typedef struct converter
{
	size_t modelCount;
	model_convert_action_t modelActions[1024];
} converter_t;

static void register_model(converter_t* converter, const char* sourcePath, const char* rootNodeName)
{
	assert(converter->modelCount < countof(converter->modelActions));
	converter->modelActions[converter->modelCount++] = (model_convert_action_t){
		.sourcePath = sourcePath,
		.rootNodeName = rootNodeName,
	};
}

static int convert(converter_t* converter)
{
	int r;

	printf("Converting...\n");

	mkdir("dat", 0700);

	FILE* resourceFile = fopen("dat/resource.bin", "wb");
	FILE* contentFile = fopen("dat/content.bin", "wb");

	if (resourceFile == NULL || contentFile == NULL)
	{
		return 1;
	}
	
	const FILEFORMAT_game_resource_header_t gameResourceHeader = {
		.version = FILEFORMAT_game_resource_VERSION,
		.modelCount = converter->modelCount,
	};
	r = fwrite(&gameResourceHeader, sizeof(gameResourceHeader), 1, resourceFile);
	assert(r == 1);
	
	FILEFORMAT_game_resource_model_entry_t* gameResourceModelEntries = calloc(converter->modelCount, sizeof(FILEFORMAT_game_resource_model_entry_t));
	assert(gameResourceModelEntries != NULL);
	
	const size_t gameResourceModelEntriesOffset = ftell(resourceFile);
	r = fwrite(gameResourceModelEntries, sizeof(FILEFORMAT_game_resource_model_entry_t), converter->modelCount, resourceFile);
	assert(r == converter->modelCount);

	for (size_t modelIndex = 0; modelIndex < converter->modelCount; ++modelIndex)
	{
		const model_convert_action_t* action = &converter->modelActions[modelIndex];

		printf("%s (root: %s)\n", action->sourcePath, action->rootNodeName);

		glb_t glb;
		if (glb_parse(&glb, action->sourcePath) != 0)
		{
			printf("error: failed to parse glb for model '%s'\n", action->sourcePath);
			return 1;
		}

		//gltf_dump(&glb.gltf);

		extracted_model_t extractedModel;
		if (extract_model(&extractedModel, &glb, action->rootNodeName) != 0)
		{
			printf("error: failed to export model '%s'\n", action->sourcePath);
			return 1;
		}
		
		FILEFORMAT_model_part_header_t partHeaders[GLTF_PARSER_MAX_NODES];
		uint32_t dataOffset = 0;

		for (size_t partIndex = 0; partIndex < extractedModel.partCount; ++partIndex)
		{
			const model_part_t* part = &extractedModel.parts[partIndex];
			const model_hierarchy_node_t* hierarchyNode = &extractedModel.hierarchy[partIndex];
			const gltf_node_t* gltfNode = hierarchyNode->gltfNode;

			FILEFORMAT_model_part_header_t partHeader = {
				.rotation		= (vec4){ gltfNode->rotation[0], gltfNode->rotation[1], gltfNode->rotation[2], gltfNode->rotation[3] },
				.translation	= (vec3){ gltfNode->translation[0], gltfNode->translation[1], gltfNode->translation[2] },
				.parentIndex	= hierarchyNode->parentIndex,
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

			partHeaders[partIndex] = partHeader;
		}

		const size_t contentOffset = ftell(contentFile);

		const FILEFORMAT_model_header_t header = {
			.contentOffset = contentOffset,
			.partCount = extractedModel.partCount,
			.dataSize = dataOffset,
		};

		gameResourceModelEntries[modelIndex] = (FILEFORMAT_game_resource_model_entry_t){
			.headerOffset = ftell(resourceFile),
		};
		
		r = fwrite(&header, sizeof(header), 1, resourceFile);
		assert(r == 1);
		r = fwrite(partHeaders, sizeof(partHeaders[0]), extractedModel.partCount, resourceFile);
		assert(r == extractedModel.partCount);

		for (int i = 0; i < extractedModel.partCount; ++i)
		{
			const model_part_t* part = &extractedModel.parts[i];

			r = fwrite(glb.buffer.data + part->indexBufferView->byteOffset, part->indexBufferView->byteLength, 1, contentFile);
			assert(r == 1);

			r = fwrite(glb.buffer.data + part->positionBufferView->byteOffset, part->positionBufferView->byteLength, 1, contentFile);
			assert(r == 1);

			r = fwrite(glb.buffer.data + part->normalBufferView->byteOffset, part->normalBufferView->byteLength, 1, contentFile);
			assert(r == 1);

			r = fwrite(part->vertexColor, part->vertexCount * sizeof(uint32_t), 1, contentFile);
			assert(r == 1);
		}
	}

	const size_t writtenGameResourceSize = ftell(resourceFile);
	const size_t writtenContentSize = ftell(contentFile);
	
	fseek(resourceFile, gameResourceModelEntriesOffset, SEEK_SET);
	r = fwrite(gameResourceModelEntries, sizeof(FILEFORMAT_game_resource_model_entry_t), converter->modelCount, resourceFile);
	assert(r == converter->modelCount);

	printf("Converter finished.\n");
	printf("Game resource: %zu bytes\n", writtenGameResourceSize);
	printf("Content: %zu bytes\n", writtenContentSize);

	fclose(resourceFile);
	fclose(contentFile);

	return 0;
}

int main(int argc, char** argv)
{
	converter_t converter = {};

	register_model(&converter, "content/tank.glb", "Tank");
	register_model(&converter, "content/colortest.glb", "Cube");

	convert(&converter);

	return 0;
}