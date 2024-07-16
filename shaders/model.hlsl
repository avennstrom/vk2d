#include "gpu_types.h"

[[vk::binding(0)]]	ConstantBuffer<gpu_frame_uniforms_t>	g_frame;
[[vk::binding(1)]]	StructuredBuffer<gpu_draw_t>			g_draws;
[[vk::binding(2)]]	ByteAddressBuffer						g_vertexBuffer;

struct VsInput
{
	[[vk::builtin("DrawIndex")]] uint drawIndex : DrawIndex;
	uint vertexId : SV_VertexID;
};

struct VsOutput
{
	float3 normal : NORMAL;
	float4 position : SV_Position;
};

VsOutput vs_main(VsInput input)
{
	VsOutput output = (VsOutput)0;
	
	const gpu_draw_t draw = g_draws[input.drawIndex];

	const float3 vertexPosition	= g_vertexBuffer.Load<float3>(draw.vertexPositionOffset + input.vertexId * sizeof(float3));
	const float3 vertexNormal	= g_vertexBuffer.Load<float3>(draw.vertexNormalOffset + input.vertexId * sizeof(float3));
	
	const float3 worldPosition	= mul(draw.transform, float4(vertexPosition, 1.0)).xyz;
	const float3 worldNormal	= mul(draw.transform, float4(vertexNormal, 0.0)).xyz;

	output.normal	= worldNormal;
	output.position	= mul(float4(worldPosition, 1.0), g_frame.matViewProj);

	output.position.y *= -1;
	return output;
}

struct FsOutput
{
	float4 color : SV_Target0;
};

FsOutput fs_main(VsOutput input)
{
	FsOutput output = (FsOutput)0;

	float3 albedo = input.normal;
	//albedo = float3(1,0,1);

	float3 color = 0.0f.rrr;
	
	//color = albedo * saturate(dot(input.normal, normalize(float3(1.0f, 0.6f, 0.4f))));
	color = albedo;

	output.color = float4(color, 1);
	return output;
}