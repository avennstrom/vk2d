#pragma once

#include <stdint.h>

#define FILEFORMAT_model_VERSION 1

typedef struct FILEFORMAT_model_header
{
	uint32_t version;
	uint32_t indexCount;
	uint32_t vertexCount;
} FILEFORMAT_model_header_t;