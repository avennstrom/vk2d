#pragma once

#include <stdint.h>
#include <stddef.h>

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
	GLTF_ATTRIBUTE_COUNT,
};

typedef struct gltf_primitive
{
	uint32_t			attributes[GLTF_ATTRIBUTE_COUNT];
	uint32_t			indices;
} gltf_primitive_t;

typedef struct gltf_mesh
{
	char				name[128];
	uint32_t			primitiveCount;
	gltf_primitive_t	primitives[64];
} gltf_mesh_t;

typedef struct gltf_accessor
{
	uint32_t				bufferView;
	uint32_t				componentType;
	uint32_t				count;
	gltf_accessor_type_t	type;
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
	uint32_t			meshCount;
	uint32_t			bufferViewCount;
	uint32_t			accessorCount;

	gltf_mesh_t			meshes[64];
	gltf_buffer_view_t	bufferViews[64];
	gltf_accessor_t		accessors[64];
} gltf_t;

void gltf_parse(gltf_t* gltf, const char* json, size_t len);
void gltf_dump(gltf_t* gltf);