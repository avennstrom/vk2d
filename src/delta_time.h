#pragma once

#define _POSIX_C_SOURCE 199309L
#include <time.h>

typedef struct delta_timer
{
	struct timespec ts;
} delta_timer_t;

void delta_timer_reset(delta_timer_t* timer);
double delta_timer_capture(delta_timer_t* timer);
