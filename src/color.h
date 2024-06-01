#pragma once

#include <stdint.h>

#define COLOR_WHITE	(0xffffffffu)
#define COLOR_BLACK	(0xff000000u)
#define COLOR_RED	(0xff0000ffu)
#define COLOR_GREEN	(0xff00ff00u)
#define COLOR_BLUE	(0xffff0000u)

#define COLOR_FROM_HEX_RGB(Hex) (0xff000000 | ((Hex & 0xff) << 16) | (Hex & 0xff00) | ((Hex >> 16) & 0xff))

#define COLOR_CHANNEL_R(Color) ((Color) & 0xffu)
#define COLOR_CHANNEL_G(Color) (((Color) >> 8u) & 0xffu)
#define COLOR_CHANNEL_B(Color) (((Color) >> 16u) & 0xffu)
#define COLOR_CHANNEL_A(Color) (((Color) >> 24u) & 0xffu)

uint32_t ColorFromFloat4(float r, float g, float b, float a);
void ColorToFloat4(float rgba[4], uint32_t color);
uint32_t ColorHash(uint32_t n);
uint32_t SrgbToLinear(uint32_t srgb);
uint32_t LerpColor(uint32_t a, uint32_t b, float t);
void GetLinearColor(float linear[4], uint32_t srgb);