#include "gpu_types.h"

[[vk::binding(0)]] Texture2D g_sceneColor;
[[vk::binding(1)]] SamplerState g_sampler;

struct VsInput
{
	uint vertexId : SV_VertexID;
};

struct VsOutput
{
	float2 uv : TEXCOORD0;
	float4 position : SV_Position;
};

VsOutput vs_main(VsInput input)
{
	float2 uv;
	if (input.vertexId == 0) {
		uv = float2(0, 0);
	}
	else if (input.vertexId == 1) {
		uv = float2(2, 0);
	}
	else {
		uv = float2(0, 2);
	}

	VsOutput output = (VsOutput)0;
	output.uv = uv;
	output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
	return output;
}

struct FsOutput
{
	float4 color : SV_Target0;
};

FsOutput fs_main(VsOutput input)
{
	FsOutput output = (FsOutput)0;

	float3 color = g_sceneColor.SampleLevel(g_sampler, input.uv, 0.0f).rgb;

	// tone mapping
	//color = color / (color + 1);

	output.color = float4(color, 1);
	return output;
}