#include "gpu_types.h"

[[vk::binding(0)]]	ConstantBuffer<gpu_frame_uniforms_t>	g_frame;
[[vk::binding(1)]]	ByteAddressBuffer						g_particles;

struct VsInput
{
	uint vertexId : SV_VertexID;
};

struct VsOutput
{
	float3 color : TEXCOORD0;
	float4 position : SV_Position;
};

float3 unpackParticleColor(uint packed)
{
	float3 color;
	color.r = (packed & 0xffu) / 255.0f;
	color.g = ((packed >> 8u) & 0xffu) / 255.0f;
	color.b = ((packed >> 16u) & 0xffu) / 255.0f;
	return color;
}

float unpackParticleSize(uint packed)
{
	return (packed & 0xffff) / (float)0xffff;
}

uint unpackParticleLayer(uint packed)
{
	return (packed >> 16) & 0xff;
}

VsOutput vs_main(VsInput input)
{
	const uint particleIndex = input.vertexId / 6;
	const uint localIndex = input.vertexId % 6;

	float2 corner;
	switch (localIndex)
	{
		case 0: corner = float2(-1.0f, -1.0f); break; // bottom left
		case 1: corner = float2(+1.0f, -1.0f); break; // bottom right
		case 2: corner = float2(-1.0f, +1.0f); break; // top left

		case 3: corner = float2(-1.0f, +1.0f); break; // top left
		case 4: corner = float2(+1.0f, -1.0f); break; // bottom right
		case 5: corner = float2(+1.0f, +1.0f); break; // top right
	}

	const gpu_particle_t particle = g_particles.Load<gpu_particle_t>(particleIndex * sizeof(gpu_particle_t));

	const float depth = unpackParticleLayer(particle.sizeAndLayer) / (float)0xffff;
	const float size = unpackParticleSize(particle.sizeAndLayer);
	const float2 center = particle.center;
	
	const float2 position = center + corner * size * 0.5f;

	VsOutput output = (VsOutput)0;
	output.color = unpackParticleColor(particle.color);
	output.position	= mul(float4(position, 0.0f, 1.0), g_frame.matViewProj);

	output.position.z = depth;
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
	output.color = float4(input.color, 1.0f);
	return output;
}