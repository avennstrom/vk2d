#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#accessor-data-types
#define GLTF_ACCESSOR_COMPONENT_TYPE_INT8	5120
#define GLTF_ACCESSOR_COMPONENT_TYPE_UINT8	5121
#define GLTF_ACCESSOR_COMPONENT_TYPE_INT16	5122
#define GLTF_ACCESSOR_COMPONENT_TYPE_UINT16	5123
#define GLTF_ACCESSOR_COMPONENT_TYPE_UINT32	5125
#define GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT	5126

typedef enum gltf_accessor_type
{
	GLTF_ACCESSOR_TYPE_SCALAR,
	GLTF_ACCESSOR_TYPE_VEC2,
	GLTF_ACCESSOR_TYPE_VEC3,
	GLTF_ACCESSOR_TYPE_VEC4,
	GLTF_ACCESSOR_TYPE_MAT2,
	GLTF_ACCESSOR_TYPE_MAT3,
	GLTF_ACCESSOR_TYPE_MAT4,
} gltf_accessor_type_t;


// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_bufferview_target
#define GLTF_BUFFER_VIEW_TARGET_ARRAY_BUFFER			34962
#define GLTF_BUFFER_VIEW_TARGET_ELEMENT_ARRAY_BUFFER	34963

// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
enum
{
	GLTF_ATTRIBUTE_POSITION,
	GLTF_ATTRIBUTE_NORMAL,
	GLTF_ATTRIBUTE_COLOR,
	GLTF_ATTRIBUTE_COUNT,
};

#define GLTF_PARSER_MAX_NODES			64
#define GLTF_PARSER_MAX_MESHES			64
#define GLTF_PARSER_MAX_BUFFER_VIEWS	64
#define GLTF_PARSER_MAX_ACCESSORS		64
#define GLTF_PARSER_NODE_MAX_CHILDREN	64
#define GLTF_PARSER_MESH_MAX_PRIMITIVES	64
#define GLTF_PARSER_MAX_NAME_LENGTH		128

typedef enum gltf_node_type
{
	GLTF_NODE_TYPE_UNDEFINED,
	GLTF_NODE_TYPE_MESH,
} gltf_node_type_t;

typedef struct gltf_node
{
	char				name[GLTF_PARSER_MAX_NAME_LENGTH];
	gltf_node_type_t	type;
	uint32_t			mesh;
	float				rotation[4];
	float				translation[3];
	uint32_t			children[GLTF_PARSER_NODE_MAX_CHILDREN];
	uint32_t			childCount;
} gltf_node_t;

typedef struct gltf_primitive
{
	uint32_t			attributes[GLTF_ATTRIBUTE_COUNT];
	uint32_t			indices;
} gltf_primitive_t;

typedef struct gltf_mesh
{
	char				name[GLTF_PARSER_MAX_NAME_LENGTH];
	uint32_t			primitiveCount;
	gltf_primitive_t	primitives[GLTF_PARSER_MESH_MAX_PRIMITIVES];
} gltf_mesh_t;

typedef struct gltf_accessor
{
	uint32_t				bufferView;
	uint32_t				componentType;
	uint32_t				count;
	gltf_accessor_type_t	type;
	bool					normalized;
} gltf_accessor_t;

typedef struct gltf_buffer_view
{
	uint32_t				buffer;
	uint32_t				byteLength;
	uint32_t				byteOffset;
	uint32_t				target;
} gltf_buffer_view_t;

typedef struct gltf
{
	uint32_t			nodeCount;
	uint32_t			meshCount;
	uint32_t			bufferViewCount;
	uint32_t			accessorCount;
	gltf_node_t			nodes[GLTF_PARSER_MAX_NODES];
	gltf_mesh_t			meshes[GLTF_PARSER_MAX_MESHES];
	gltf_buffer_view_t	bufferViews[GLTF_PARSER_MAX_BUFFER_VIEWS];
	gltf_accessor_t		accessors[GLTF_PARSER_MAX_ACCESSORS];
} gltf_t;

void gltf_parse(gltf_t* gltf, const char* json, size_t len);
void gltf_dump(gltf_t* gltf);