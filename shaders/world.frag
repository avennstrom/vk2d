#version 450
#extension GL_GOOGLE_include_directive : require
#include "gpu_types.h"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPos;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec2 fragUv;
layout(location = 4) in flat uint inFlags0;
layout(location = 5) in flat uint inFlags1;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameUniformBlock { gpu_frame_uniforms_t g_frame; };
layout(set = 0, binding = 1, std140) readonly buffer LightBuffer GPU_LIGHT_BUFFER_BLOCK g_scene;
layout(set = 0, binding = 2) uniform sampler2DArray g_spotLightAtlas;

void calculatePointLight(inout vec3 color, vec3 albedo, gpu_point_light_t light)
{
    const float d = length(fragPos - light.pos);
    const float t = d / light.radius;
    
    if (t < 1)
    {
        const float atten = (1.0 - t) * (1.0 - t);
        const float dt = max(0,dot(normalize(light.pos - fragPos), fragNormal));
        color += dt * atten * albedo * light.color.rgb;
    }
}

void calculateSpotLight(inout vec3 color, vec3 albedo, gpu_spot_light_t light, uint lightIndex)
{
    const vec3 toLight = light.pos - fragPos;
    const float d = length(toLight);
    const float t = d / light.range;

    const float ndotl = dot(fragNormal, normalize(toLight));

    if (t > 1 || ndotl <= 0) {
        return;
    }
    
    const float falloff = (1.0 - t) * (1.0 - t);

    float visibility = 1;
    
    mat4 viewProjection = g_scene.spotLightMatrices[lightIndex];
    
    vec4 ndc = vec4(fragPos, 1) * viewProjection;
    ndc.xyz /= ndc.w;

    vec2 uv = ndc.xy * vec2(0.5, -0.5) + 0.5;
    //uv.y = 1.0 - uv.y;

    const float coneInfluence = max(0, 1.0 - (length(ndc.xy) / 0.5));

    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1 || ndc.z < 0 || ndc.z > 1) {
        return;
    }
    
    float dtex = texture(g_spotLightAtlas, vec3(uv, lightIndex)).r;
    float depth = ndc.z;
    
    //visibility = abs(dtex - depth);
    visibility = (depth < dtex) ? 1 : 0;

    // color.r = dtex;
    // color.g = depth;
    // color.b = visibility;

    //color.rg = uv;
    //color.r = abs(depth - dtex) * 10;
    
    //color.r = abs(depth - dtex) * 100;
    //color.r = fract(dtex * 128);
    //color.g = fract(depth * 128);
    //color.r = max(0, dtex - depth) * 100;

    //color += ndotl * visibility;

    color += ndotl * coneInfluence * falloff * visibility * light.color;

    //color += visibility * dt * atten * albedo * light.color.rgb * intensity;
}

void main() {
    vec3 albedo = fragColor;

    vec3 color = 0.0.rrr;

    //color += albedo;

    for (uint i = 0; i < g_frame.pointLightCount; ++i)
    {
       calculatePointLight(color, albedo, g_scene.pointLights[i]);
    }

    for (uint i = 0; i < g_frame.spotLightCount; ++i)
    {
        calculateSpotLight(color, albedo, g_scene.spotLights[i], i);
    }
    
    outColor = vec4(color, 1.0);
}
