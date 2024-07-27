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
	float3 color : TEXCOORD0;
	float4 position : SV_Position;
};

float3 unpackVertexColor(uint packed)
{
	float3 color;
	color.r = (packed & 0xffu) / 255.0f;
	color.g = ((packed >> 8u) & 0xffu) / 255.0f;
	color.b = ((packed >> 16u) & 0xffu) / 255.0f;
	//color.a = packed >> 24u;
	return color;
}

float3 srgbToLinear(float3 srgb)
{
	return pow(srgb, 2.2f);
}

float3 linearToSrgb(float3 linearColor)
{
	return pow(linearColor, 1.0f/2.2f);
}

VsOutput vs_main(VsInput input)
{
	VsOutput output = (VsOutput)0;
	
	const gpu_draw_t draw = g_draws[input.drawIndex];

	const float3	vertexPosition		= g_vertexBuffer.Load<float3>(draw.vertexPositionOffset + input.vertexId * sizeof(float3));
	const float3	vertexNormal		= g_vertexBuffer.Load<float3>(draw.vertexNormalOffset + input.vertexId * sizeof(float3));
	const uint		vertexColorPacked	= g_vertexBuffer.Load(draw.vertexColorOffset + input.vertexId * sizeof(uint));

	const float3 worldPosition	= mul(draw.transform, float4(vertexPosition, 1.0)).xyz;
	const float3 worldNormal	= mul(draw.transform, float4(vertexNormal, 0.0)).xyz;

	output.normal	= worldNormal;
	output.color	= unpackVertexColor(vertexColorPacked);
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

	float3 albedo = input.color;
	//albedo = input.normal;

	float3 color = 0.0f.rrr;
	
	color = albedo * saturate(dot(input.normal, normalize(float3(1.0f, 0.6f, 0.4f))));
	//color = albedo;

	output.color = float4(color, 1);
	return output;
}