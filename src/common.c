#include "common.h"

void StepInDirection(short3* pos, direction_t dir)
{
	switch (dir) {
		case Direction_North:	pos->z -= 1; break;
		case Direction_South:	pos->z += 1; break;
		case Direction_East:	pos->x += 1; break;
		case Direction_West:	pos->x -= 1; break;
		case Direction_Up:		pos->y += 1; break;
		case Direction_Down:	pos->y -= 1; break;
	}
}

direction_t OppositeDirection(direction_t dir)
{
	switch (dir) {
		case Direction_North:	return Direction_South;
		case Direction_East:	return Direction_West;
		case Direction_South:	return Direction_North;
		case Direction_West:	return Direction_East;
		case Direction_Up:		return Direction_Down;
		case Direction_Down:	return Direction_Up;
	}
}