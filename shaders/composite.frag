#version 450

layout(binding = 0) uniform sampler2D g_sceneColor;

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(g_sceneColor, in_uv).rgb;
    color = color / (color + 1);

    outColor = vec4(color, 1.0);
}
