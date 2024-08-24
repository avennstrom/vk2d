#include "gpu_types.h"

[[vk::binding(0)]]	ConstantBuffer<gpu_debug_renderer_uniforms_t>	g_uniforms;

struct VsInput
{
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float3 color : COLOR;
};

struct VsOutput
{
	[[vk::builtin("PointSize")]] float pointSize : TEXCOORD0;
	float3 color : TEXCOORD1;
	float4 position : SV_Position;
};

VsOutput vs_main(VsInput input)
{
	VsOutput output = (VsOutput)0;
	output.pointSize	= 16.0f;
	output.color		= input.color;
	output.position		= mul(float4(input.position, 1.0), g_uniforms.matViewProj);
	output.position.y	= -output.position.y;
	return output;
}

struct FsOutput
{
	float4 color : SV_Target0;
};

FsOutput fs_main(VsOutput input)
{
	FsOutput output = (FsOutput)0;
	output.color = float4(input.color, 1);
	return output;
}