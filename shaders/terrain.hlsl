#include "gpu_types.h"

[[vk::binding(0)]]	ConstantBuffer<gpu_frame_uniforms_t>	g_frame;
[[vk::binding(1)]]	StructuredBuffer<float>					g_heightData;

struct VsInput
{
	uint vertexId : SV_VertexID;
};

struct VsOutput
{
	float4 position : SV_Position;
};

float rand(int seed) {
	seed = (seed << 13) ^ seed;
	return abs((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) / 2147483648.0;
}

VsOutput vs_main(VsInput input)
{
	const uint x = input.vertexId % TERRAIN_PATCH_INDEX_STRIDE;
	const uint z = (input.vertexId / TERRAIN_PATCH_INDEX_STRIDE) % TERRAIN_PATCH_INDEX_STRIDE;

	const float height = g_heightData[input.vertexId];
	//const float height = rand(input.vertexId);
	
	VsOutput output = (VsOutput)0;
	output.position	= mul(float4(x, height, z, 1.0), g_frame.matViewProj);

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
	float3 color = hash3(primitiveId);

	FsOutput output = (FsOutput)0;
	output.color = float4(color, 1.0f);
	return output;
}