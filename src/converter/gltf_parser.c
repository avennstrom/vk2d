#include "gltf_parser.h"
#include "jsmn.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

typedef struct gltf_parser
{
	const char*	json;
	jsmntok_t*	tokens;
	uint32_t	pos;
	gltf_t*		gltf;
} gltf_parser_t;

static void tokput(const gltf_parser_t* parser, const char* comment, jsmntok_t tok)
{
#if 0
	printf("%s: %.*s\n", comment, tok.end - tok.start, parser->json + tok.start);
#endif
}

static void gltf_json_parser_skip_value(gltf_parser_t* parser);

static bool jsoneq(gltf_parser_t* parser, const char *s) {
	const jsmntok_t tok = parser->tokens[parser->pos];
	const int len = tok.end - tok.start;
	if (tok.type == JSMN_STRING && (int)strlen(s) == len && strncmp(parser->json + tok.start, s, len) == 0) {
		return true;
	}
	return false;
}

static jsmntok_t gltf_json_parser_eat_object(gltf_parser_t* parser)
{
	assert(parser->tokens[parser->pos].type == JSMN_OBJECT);
	return parser->tokens[parser->pos++];
}

static jsmntok_t gltf_json_parser_eat_array(gltf_parser_t* parser)
{
	assert(parser->tokens[parser->pos].type == JSMN_ARRAY);
	return parser->tokens[parser->pos++];
}

static uint32_t gltf_json_parser_eat_uint32(gltf_parser_t* parser)
{
	const jsmntok_t tok = parser->tokens[parser->pos++];
	assert(tok.type == JSMN_PRIMITIVE);
	const int len = tok.end - tok.start;
	char buf[64];
	memcpy(buf, parser->json + tok.start, len);
	buf[len] = '\0';
	return strtoul(buf, NULL, 10);
}

static float gltf_json_parser_eat_float(gltf_parser_t* parser)
{
	const jsmntok_t tok = parser->tokens[parser->pos++];
	assert(tok.type == JSMN_PRIMITIVE);
	const int len = tok.end - tok.start;
	char buf[64];
	memcpy(buf, parser->json + tok.start, len);
	buf[len] = '\0';
	return strtof(buf, NULL);
}

static void gltf_json_parser_skip_key(gltf_parser_t* parser)
{
	tokput(parser, "skip_key", parser->tokens[parser->pos]);
	assert(parser->tokens[parser->pos].type == JSMN_STRING);
	++parser->pos;
}

static bool gltf_json_parser_eat_key(gltf_parser_t* parser, const char* s)
{
	if (jsoneq(parser, s))
	{
		gltf_json_parser_skip_key(parser);
		return true;
	}
	return false;
}

static void gltf_json_parser_eat_string(char* str, gltf_parser_t* parser)
{
	const jsmntok_t tok = parser->tokens[parser->pos++];
	assert(tok.type == JSMN_STRING);
	const int len = tok.end - tok.start;
	memcpy(str, parser->json + tok.start, len);
	str[len] = '\0';
}

static void gltf_json_parser_skip_field(gltf_parser_t* parser)
{
	gltf_json_parser_skip_key(parser);
	gltf_json_parser_skip_value(parser);
}

static void gltf_json_parser_skip_object(gltf_parser_t* parser)
{
	const jsmntok_t tok = parser->tokens[parser->pos++];
	const int n = tok.size;

	assert(tok.type == JSMN_OBJECT);

	tokput(parser, "skip_object", tok);

	for (int i = 0; i < n; ++i)
	{
		gltf_json_parser_skip_field(parser);
	}
}

static void gltf_json_parser_skip_array(gltf_parser_t* parser)
{
	const jsmntok_t tok = parser->tokens[parser->pos++];
	const int n = tok.size;

	assert(tok.type == JSMN_ARRAY);

	tokput(parser, "skip_array", tok);

	for (int i = 0; i < n; ++i)
	{
		gltf_json_parser_skip_value(parser);
	}
}

static void gltf_json_parser_skip_value(gltf_parser_t* parser)
{
	const jsmntok_t tok = parser->tokens[parser->pos];
	//tokput(parser, "skip_value", parser->tokens[parser->pos]);
	if (tok.type == JSMN_OBJECT)
	{
		gltf_json_parser_skip_object(parser);
	}
	else if (tok.type == JSMN_ARRAY)
	{
		gltf_json_parser_skip_array(parser);
	}
	else if (tok.type == JSMN_STRING || tok.type == JSMN_PRIMITIVE)
	{
		++parser->pos;
	}
	else
	{
		assert(0);
	}
}

static void gltf_json_parser_parse_bufferView(gltf_parser_t* parser)
{
	gltf_buffer_view_t* bufferView = &parser->gltf->bufferViews[parser->gltf->bufferViewCount++];
	
	const jsmntok_t tok = gltf_json_parser_eat_object(parser);
	tokput(parser, "parse_bufferView", tok);

	for (int i = 0; i < tok.size; ++i)
	{
		if (gltf_json_parser_eat_key(parser, "buffer"))
		{
			bufferView->buffer = gltf_json_parser_eat_uint32(parser);
		}
		else if (gltf_json_parser_eat_key(parser, "byteLength"))
		{
			bufferView->byteLength = gltf_json_parser_eat_uint32(parser);
		}
		else if (gltf_json_parser_eat_key(parser, "byteOffset"))
		{
			bufferView->byteOffset = gltf_json_parser_eat_uint32(parser);
		}
		else if (gltf_json_parser_eat_key(parser, "target"))
		{
			bufferView->target = gltf_json_parser_eat_uint32(parser);
		}
		else
		{
			gltf_json_parser_skip_field(parser);
		}
	}
}

static void gltf_json_parser_parse_bufferViews(gltf_parser_t* parser)
{
	const jsmntok_t arr = gltf_json_parser_eat_array(parser);
	for (int i = 0; i < arr.size; ++i)
	{
		gltf_json_parser_parse_bufferView(parser);
	}
}

static void gltf_json_parser_parse_accessors(gltf_parser_t* parser)
{
	const jsmntok_t arr = gltf_json_parser_eat_array(parser);
	for (int i = 0; i < arr.size; ++i)
	{
		gltf_accessor_t* accessor = &parser->gltf->accessors[parser->gltf->accessorCount++];
	
		const jsmntok_t tok = gltf_json_parser_eat_object(parser);
		tokput(parser, "parse_accessor", tok);

		for (int accessor_i = 0; accessor_i < tok.size; ++accessor_i)
		{
			if (gltf_json_parser_eat_key(parser, "bufferView"))
			{
				accessor->bufferView = gltf_json_parser_eat_uint32(parser);
			}
			else if (gltf_json_parser_eat_key(parser, "componentType"))
			{
				accessor->componentType = gltf_json_parser_eat_uint32(parser);
			}
			else if (gltf_json_parser_eat_key(parser, "count"))
			{
				accessor->count = gltf_json_parser_eat_uint32(parser);
			}
			else if (gltf_json_parser_eat_key(parser, "type"))
			{
				if (jsoneq(parser, "SCALAR"))		{ accessor->type = GLTF_ACCESSOR_TYPE_SCALAR; }
				else if (jsoneq(parser, "VEC2"))	{ accessor->type = GLTF_ACCESSOR_TYPE_VEC2; }
				else if (jsoneq(parser, "VEC3"))	{ accessor->type = GLTF_ACCESSOR_TYPE_VEC3; }
				else if (jsoneq(parser, "VEC4"))	{ accessor->type = GLTF_ACCESSOR_TYPE_VEC4; }
				else if (jsoneq(parser, "MAT2"))	{ accessor->type = GLTF_ACCESSOR_TYPE_MAT2; }
				else if (jsoneq(parser, "MAT3"))	{ accessor->type = GLTF_ACCESSOR_TYPE_MAT3; }
				else if (jsoneq(parser, "MAT4"))	{ accessor->type = GLTF_ACCESSOR_TYPE_MAT4; }
				else
				{
					assert(0 && "Invalid accessor type");
				}
				++parser->pos;
			}
			else
			{
				gltf_json_parser_skip_field(parser);
			}
		}
	}
}

static void gltf_json_parser_parse_primitive(gltf_parser_t* parser, gltf_mesh_t* mesh)
{
	gltf_primitive_t* primitive = &mesh->primitives[mesh->primitiveCount++];

	memset(primitive->attributes, 0xffffffff, sizeof(primitive->attributes));
	
	const jsmntok_t tok = gltf_json_parser_eat_object(parser);
	tokput(parser, "parse_primitive", tok);

	for (int i = 0; i < tok.size; ++i)
	{
		if (gltf_json_parser_eat_key(parser, "indices"))
		{
			primitive->indices = gltf_json_parser_eat_uint32(parser);
		}
		else if (gltf_json_parser_eat_key(parser, "attributes"))
		{
			const jsmntok_t attrtok = gltf_json_parser_eat_object(parser);
			for (int j = 0; j < attrtok.size; ++j)
			{
				if (gltf_json_parser_eat_key(parser, "POSITION"))
				{
					primitive->attributes[GLTF_ATTRIBUTE_POSITION] = gltf_json_parser_eat_uint32(parser);
				}
				else if (gltf_json_parser_eat_key(parser, "NORMAL"))
				{
					primitive->attributes[GLTF_ATTRIBUTE_NORMAL] = gltf_json_parser_eat_uint32(parser);
				}
				//else if (gltf_json_parser_eat_key(parser, "COLOR_0"))
				else if (gltf_json_parser_eat_key(parser, "_COL"))
				{
					primitive->attributes[GLTF_ATTRIBUTE_COLOR] = gltf_json_parser_eat_uint32(parser);
				}
				else
				{
					gltf_json_parser_skip_field(parser);
				}
			}
		}
		else
		{
			gltf_json_parser_skip_field(parser);
		}
	}
}

static void gltf_json_parser_parse_primitives(gltf_parser_t* parser, gltf_mesh_t* mesh)
{
	const jsmntok_t arr = gltf_json_parser_eat_array(parser);
	for (int i = 0; i < arr.size; ++i)
	{
		gltf_json_parser_parse_primitive(parser, mesh);
	}
}

static void gltf_json_parser_parse_meshes(gltf_parser_t* parser)
{
	const jsmntok_t arr = gltf_json_parser_eat_array(parser);
	for (int i = 0; i < arr.size; ++i)
	{
		gltf_mesh_t* mesh = &parser->gltf->meshes[parser->gltf->meshCount++];
	
		const jsmntok_t tok = gltf_json_parser_eat_object(parser);
		tokput(parser, "parse_mesh", tok);

		for (int mesh_i = 0; mesh_i < tok.size; ++mesh_i)
		{
			if (gltf_json_parser_eat_key(parser, "name"))
			{
				gltf_json_parser_eat_string(mesh->name, parser);
			}
			else if (gltf_json_parser_eat_key(parser, "primitives"))
			{
				gltf_json_parser_parse_primitives(parser, mesh);
			}
			else
			{
				gltf_json_parser_skip_field(parser);
			}
		}
	}
}

static void gltf_json_parser_parse_node(gltf_parser_t* parser)
{
	gltf_node_t* node = &parser->gltf->nodes[parser->gltf->nodeCount++];
	
	const jsmntok_t tok = gltf_json_parser_eat_object(parser);
	tokput(parser, "parse_node", tok);

	for (int i = 0; i < tok.size; ++i)
	{
		if (gltf_json_parser_eat_key(parser, "name"))
		{
			gltf_json_parser_eat_string(node->name, parser);
		}
		else if (gltf_json_parser_eat_key(parser, "children"))
		{
			const jsmntok_t arr = gltf_json_parser_eat_array(parser);
			for (int j = 0; j < arr.size; ++j)
			{
				assert(node->childCount < GLTF_PARSER_NODE_MAX_CHILDREN);
				node->children[node->childCount++] = gltf_json_parser_eat_uint32(parser);
			}
		}
		else if (gltf_json_parser_eat_key(parser, "rotation"))
		{
			const jsmntok_t arr = gltf_json_parser_eat_array(parser);
			assert(arr.size == 4);
			node->rotation[0] = gltf_json_parser_eat_float(parser);
			node->rotation[1] = gltf_json_parser_eat_float(parser);
			node->rotation[2] = gltf_json_parser_eat_float(parser);
			node->rotation[3] = gltf_json_parser_eat_float(parser);
		}
		else if (gltf_json_parser_eat_key(parser, "translation"))
		{
			const jsmntok_t arr = gltf_json_parser_eat_array(parser);
			assert(arr.size == 3);
			node->translation[0] = gltf_json_parser_eat_float(parser);
			node->translation[1] = gltf_json_parser_eat_float(parser);
			node->translation[2] = gltf_json_parser_eat_float(parser);
		}
		else if (gltf_json_parser_eat_key(parser, "mesh"))
		{
			node->type = GLTF_NODE_TYPE_MESH;
			node->mesh = gltf_json_parser_eat_uint32(parser);
		}
		else
		{
			gltf_json_parser_skip_field(parser);
		}
	}

	assert(node->type != GLTF_NODE_TYPE_UNDEFINED);
}

static void gltf_json_parser_parse_nodes(gltf_parser_t* parser)
{
	const jsmntok_t arr = gltf_json_parser_eat_array(parser);
	for (int i = 0; i < arr.size; ++i)
	{
		gltf_json_parser_parse_node(parser);
	}
}

void gltf_parse(gltf_t* gltf, const char* json, size_t len)
{
	jsmn_parser jsmn;
	jsmn_init(&jsmn);

	jsmntok_t tokens[16 * 1024];
	const int tokenCount = jsmn_parse(&jsmn, json, len, tokens, 16 * 1024);
	assert(tokenCount > 0);

	gltf_parser_t parser = {
		.json	= json,
		.tokens	= tokens,
		.gltf	= gltf,
	};

	const jsmntok_t root = gltf_json_parser_eat_object(&parser);
	
	for (int i = 0; i < root.size; ++i)
	{
		if (gltf_json_parser_eat_key(&parser, "bufferViews"))
		{
			gltf_json_parser_parse_bufferViews(&parser);
		}
		else if (gltf_json_parser_eat_key(&parser, "accessors"))
		{
			gltf_json_parser_parse_accessors(&parser);
		}
		else if (gltf_json_parser_eat_key(&parser, "meshes"))
		{
			gltf_json_parser_parse_meshes(&parser);
		}
		else if (gltf_json_parser_eat_key(&parser, "nodes"))
		{
			gltf_json_parser_parse_nodes(&parser);
		}
		else
		{
			gltf_json_parser_skip_field(&parser);
		}
	}
}

void gltf_dump(gltf_t* gltf)
{
	for (int i = 0; i < gltf->nodeCount; ++i)
	{
		const gltf_node_t* node = &gltf->nodes[i];
		printf("nodes[%d].name: %s\n", i, node->name);
		if (node->type == GLTF_NODE_TYPE_MESH)
		{
			printf("nodes[%d].type: %s\n", i, "MESH");
			printf("nodes[%d].mesh: %u\n", i, node->mesh);
		}
		printf("nodes[%d].rotation: {%f, %f, %f, %f}\n", i, node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
		printf("nodes[%d].translation: {%f, %f, %f}\n", i, node->translation[0], node->translation[1], node->translation[2]);
		for (int j = 0; j < node->childCount; ++j)
		{
			const uint32_t child = node->children[j];
			printf("nodes[%d].children[%d]: %u\n", i, j, child);
		}
	}

	for (int i = 0; i < gltf->meshCount; ++i)
	{
		const gltf_mesh_t* mesh = &gltf->meshes[i];
		printf("meshes[%d].name: %s\n", i, mesh->name);
		for (int j = 0; j < mesh->primitiveCount; ++j)
		{
			const gltf_primitive_t* prim = &mesh->primitives[j];
			printf("meshes[%d].primitives[%d].attributes[POSITION]: %u\n", i, j, prim->attributes[GLTF_ATTRIBUTE_POSITION]);
			printf("meshes[%d].primitives[%d].attributes[NORMAL]: %u\n", i, j, prim->attributes[GLTF_ATTRIBUTE_NORMAL]);
			printf("meshes[%d].primitives[%d].attributes[COLOR]: %u\n", i, j, prim->attributes[GLTF_ATTRIBUTE_COLOR]);
			printf("meshes[%d].primitives[%d].indices: %u\n", i, j, prim->indices);
		}
	}

	for (int i = 0; i < gltf->accessorCount; ++i)
	{
		gltf_accessor_t* accessor = &gltf->accessors[i];
		printf("accessors[%d].bufferView: %u\n", i, accessor->bufferView);
		printf("accessors[%d].componentType: %u\n", i, accessor->componentType);
		printf("accessors[%d].count: %u\n", i, accessor->count);
		printf("accessors[%d].type: %u\n", i, accessor->type);
	}

	for (int i = 0; i < gltf->bufferViewCount; ++i)
	{
		gltf_buffer_view_t* bufferView = &gltf->bufferViews[i];
		printf("bufferViews[%d].buffer: %u\n", i, bufferView->buffer);
		printf("bufferViews[%d].byteLength: %u\n", i, bufferView->byteLength);
		printf("bufferViews[%d].byteOffset: %u\n", i, bufferView->byteOffset);
		printf("bufferViews[%d].target: %u\n", i, bufferView->target);
	}
}