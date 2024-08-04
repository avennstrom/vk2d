#include "gpu_types.h"

[[vk::binding(0)]]	ConstantBuffer<gpu_frame_uniforms_t>	g_frame;
[[vk::binding(1)]]	ByteAddressBuffer						g_vertexPositionBuffer;
[[vk::binding(2)]]	ByteAddressBuffer						g_vertexColorBuffer;

struct VsInput
{
	uint vertexId : SV_VertexID;
};

struct VsOutput
{
	float3 color : TEXCOORD0;
	float4 position : SV_Position;
};

float3 unpackVertexColor(uint packed)
{
	float3 color;
	color.r = (packed & 0xffu) / 255.0f;
	color.g = ((packed >> 8u) & 0xffu) / 255.0f;
	color.b = ((packed >> 16u) & 0xffu) / 255.0f;
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

float wind(float t)
{
	return sin(t * 2.0f) * sin(t * 3.0f) * cos(t * 5.0f) * cos(t * 7.0f);
}

VsOutput vs_main(VsInput input)
{
	VsOutput output = (VsOutput)0;

	float3			vertexPosition			= g_vertexPositionBuffer.Load<float3>(input.vertexId * sizeof(float3));
	const uint		vertexColorPacked		= g_vertexColorBuffer.Load(input.vertexId * sizeof(uint));
	const float		vertexAnimationWeight	= (vertexColorPacked >> 24) / 255.0f;
	
	vertexPosition.x += wind(g_frame.elapsedTime * 0.001f * 0.5f + vertexPosition.x * 0.8f) * vertexAnimationWeight * 0.1f;

	output.color	= unpackVertexColor(vertexColorPacked);
	output.position	= mul(float4(vertexPosition, 1.0), g_frame.matViewProj);

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
	
	//color = albedo * saturate(dot(input.normal, normalize(float3(1.0f, 0.6f, 0.4f))));
	//color = albedo;

	color = albedo;

	output.color = float4(color, 1);
	return output;
}