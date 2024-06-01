#pragma once

#include "types.h"

#define C
#include "../shaders/common.h"
#undef C

#define FRAME_COUNT (2)

#define MAX(a, b) (((a) < (b)) ? (b) : (a))
#define MIN(a, b) (((b) < (a)) ? (b) : (a))

typedef enum direction {
	Direction_North = 0,
	Direction_East  = 1,
	Direction_South = 2,
	Direction_West  = 3,
	Direction_Up    = 4,
	Direction_Down  = 5,
} direction_t;

#define DMASK_N 0b000001
#define DMASK_E 0b000010
#define DMASK_S 0b000100
#define DMASK_W 0b001000
#define DMASK_U 0b010000
#define DMASK_D 0b100000

#define DMASK_NE (DMASK_N | DMASK_E)
#define DMASK_SE (DMASK_S | DMASK_E)
#define DMASK_SW (DMASK_S | DMASK_W)
#define DMASK_NW (DMASK_N | DMASK_W)

void StepInDirection(short3* pos, direction_t dir);
direction_t OppositeDirection(direction_t dir);