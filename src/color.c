#include "color.h"

#include <math.h>

uint32_t ColorChannelFromFloat(float c)
{
	return (uint32_t)(c * 0xffu);
}

uint32_t ColorFromFloat4(float r, float g, float b, float a)
{
	uint32_t color = 0u;
	color |= ColorChannelFromFloat(r);
	color |= ColorChannelFromFloat(g) << 8u;
	color |= ColorChannelFromFloat(b) << 16u;
	color |= ColorChannelFromFloat(a) << 24u;
	return color;
}

void ColorToFloat4(float rgba[4], uint32_t color)
{
	rgba[0] = COLOR_CHANNEL_R(color) / 255.0f;
	rgba[1] = COLOR_CHANNEL_G(color) / 255.0f;
	rgba[2] = COLOR_CHANNEL_B(color) / 255.0f;
	rgba[3] = COLOR_CHANNEL_A(color) / 255.0f;
}

uint32_t ColorHash(uint32_t n)
{
	n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 789221U) + 1376312589U;
	
	uint32_t kx = (n * n) & 0x7fffffffu;
	uint32_t ky = (n * 16807U) & 0x7fffffffu;
	uint32_t kz = (n * 48271U) & 0x7fffffffu;
	float r = kx / (float)0x7fffffffu;
	float g = ky / (float)0x7fffffffu;
	float b = kz / (float)0x7fffffffu;
    return ColorFromFloat4(r, g, b, 1.0f);
}

float SrgbToLinearF(float srgb)
{
    return (srgb <= 0.04045f) ? srgb / 12.92f : powf((srgb + 0.055f) / 1.055f, 2.4f);
}

void GetLinearColor(float linear[4], uint32_t srgb)
{
	linear[0] = SrgbToLinearF(COLOR_CHANNEL_R(srgb) / 255.0f);
	linear[1] = SrgbToLinearF(COLOR_CHANNEL_G(srgb) / 255.0f);
	linear[2] = SrgbToLinearF(COLOR_CHANNEL_B(srgb) / 255.0f);
	linear[3] = srgb;
}

uint32_t SrgbToLinear(uint32_t srgb)
{
	float f[4];
	GetLinearColor(f, srgb);
	return ColorFromFloat4(f[0], f[1], f[2], COLOR_CHANNEL_A(srgb));
}

static float Lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

uint32_t LerpColor(uint32_t a, uint32_t b, float t)
{
	float A[4];
	float B[4];
	ColorToFloat4(A, a);
	ColorToFloat4(B, b);
	
	float R[4];
	R[0] = Lerp(A[0], B[0], t);
	R[1] = Lerp(A[1], B[1], t);
	R[2] = Lerp(A[2], B[2], t);
	R[3] = Lerp(A[3], B[3], t);
	return ColorFromFloat4(R[0], R[1], R[2], R[3]);
}