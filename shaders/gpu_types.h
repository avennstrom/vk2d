#define MAX_DRAWS (1024)
#define MAX_POINT_LIGHTS (64)
#define MAX_SPOT_LIGHTS (32)

#ifdef __STDC__
typedef struct gpu_draw_t gpu_draw_t;
typedef struct gpu_point_light_t gpu_point_light_t;
typedef struct gpu_spot_light_t gpu_spot_light_t;
typedef struct gpu_frame_uniforms_t gpu_frame_uniforms_t;
typedef struct gpu_light_buffer_t gpu_light_buffer_t;
#endif

#ifdef __HLSL__
typedef float2 vec2;
typedef float3 vec3;
typedef float4 vec4;
typedef float4x4 mat4;
#endif

struct gpu_draw_t
{
	uint	indexCount;
	uint	instanceCount;
	uint	firstIndex;
	int		vertexOffset;

	uint	firstInstance;
	uint	vertexPositionOffset;
	uint	vertexNormalOffset;
	uint	_pad0;

	mat4	transform;
};

struct gpu_point_light_t
{
	vec3	pos;
	float	radius;
	vec3	color;
	float	_pad0;
};

struct gpu_spot_light_t
{
	vec3	pos;
	float	range;
	vec3	color;
	float	_pad0;
};

struct gpu_frame_uniforms_t
{
	mat4	matViewProj;
	uint	drawCount;
	uint	pointLightCount;
	uint	spotLightCount;
	uint	_pad0;
};

#define GPU_LIGHT_BUFFER_BLOCK \
	{ \
		gpu_point_light_t pointLights[MAX_POINT_LIGHTS]; \
		gpu_spot_light_t spotLights[MAX_SPOT_LIGHTS]; \
		mat4 spotLightMatrices[MAX_SPOT_LIGHTS]; \
	}

#ifdef __STDC__
struct gpu_light_buffer_t GPU_LIGHT_BUFFER_BLOCK;
#endif

#ifdef __STDC__
_Static_assert(sizeof(gpu_draw_t) == 96, "");
_Static_assert(sizeof(gpu_point_light_t) == 32, "");
_Static_assert(sizeof(gpu_spot_light_t) == 32, "");
_Static_assert(sizeof(gpu_frame_uniforms_t) == 80, "");
_Static_assert(sizeof(gpu_light_buffer_t) == MAX_POINT_LIGHTS*sizeof(gpu_point_light_t) + MAX_SPOT_LIGHTS*sizeof(gpu_spot_light_t) + MAX_SPOT_LIGHTS*sizeof(mat4), "");
#endif