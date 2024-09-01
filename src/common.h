#pragma once

#include "types.h"

#include "../shaders/gpu_types.h"

#define FRAME_COUNT (2)

#define TICK_RATE		(60)
#define DELTA_TIME_MS	(1000.0f / TICK_RATE)

#define PARALLAX_LAYER_COUNT 16

#define CAMERA_FOV 60.0f
#define CAMERA_NEAR 0.01f
#define CAMERA_FAR 256.0f
#define CAMERA_OFFSET 7.0f