#include "gpu_types.h"

[[vk::binding(0)]]	ConstantBuffer<gpu_frame_uniforms_t>	g_frame;
[[vk::binding(1)]]	StructuredBuffer<float>					g_heightData;
[[vk::binding(2)]]	StructuredBuffer<float3>				g_normalData;

struct VsInput
{
	uint vertexId : SV_VertexID;
	uint instanceId : SV_InstanceID;
};

struct VsOutput
{
	float height : TEXCOORD0;
	float3 normal : TEXCOORD1;
	uint instanceId : TEXCOORD2;
	float4 position : SV_Position;
};

float rand(int seed) {
	seed = (seed << 13) ^ seed;
	return abs((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) / 2147483648.0;
}

VsOutput vs_main(VsInput input)
{
	uint x = input.vertexId % TERRAIN_PATCH_INDEX_STRIDE;
	uint z = (input.vertexId / TERRAIN_PATCH_INDEX_STRIDE) % TERRAIN_PATCH_INDEX_STRIDE;
	
	x += ( input.instanceId % TERRAIN_PATCH_COUNT ) * TERRAIN_PATCH_SIZE;
	z += ( ( input.instanceId / TERRAIN_PATCH_COUNT ) % TERRAIN_PATCH_COUNT ) * TERRAIN_PATCH_SIZE;

	const float height = g_heightData[input.vertexId + input.instanceId * TERRAIN_PATCH_VERTEX_COUNT];
	const float3 normal = g_normalData[input.vertexId + input.instanceId * TERRAIN_PATCH_VERTEX_COUNT];
	//const float height = rand(input.vertexId);
	
	VsOutput output = (VsOutput)0;
	output.position	= mul(float4(x, height, z, 1.0), g_frame.matViewProj);
	output.height = height;
	output.normal = normal;
	output.instanceId = input.instanceId;

	output.position.y *= -1;
	return output;
}

float3 hash3(int triangleID) {
	return float3(
		rand(triangleID), 
		rand(triangleID + 1u), 
		rand(triangleID + 2u)
	);
}

struct FsOutput
{
	float4 color : SV_Target0;
};

FsOutput fs_main(VsOutput input, uint primitiveId : SV_PrimitiveID)
{
	float3 albedo = hash3(input.instanceId);
	//albedo = hash3(primitiveId);

	const float h = (input.height + 10.0f) / 10.0f;
	albedo = lerp(float3(0.7f, 0.4f, 0.1f), float3(0.5f, 0.5f, 0.6f), h);

	float3 color = 0.0f.xxx;
	
	const float3 sunDirection = float3(0.6f, -0.4f, 0.2f);
	const float sunIntensity = max(0, dot(input.normal, -sunDirection));

	color = albedo * sunIntensity;
	//color = input.normal;

	FsOutput output = (FsOutput)0;
	output.color = float4(color, 1.0f);
	return output;
}