#pragma once

#include <time.h>

typedef struct delta_timer
{
	struct timespec ts;
} delta_timer_t;

void ResetDeltaTime(delta_timer_t* timer);
double CaptureDeltaTime(delta_timer_t* timer);
