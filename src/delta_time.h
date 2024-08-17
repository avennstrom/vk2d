#pragma once

#include <time.h>

typedef struct delta_timer
{
	struct timespec start;
	struct timespec ts;
} delta_timer_t;

void delta_timer_reset(delta_timer_t* timer);
void delta_timer_capture(double* deltaTime, double* elapsedTime, delta_timer_t* timer);
