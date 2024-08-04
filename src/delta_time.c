#include "delta_time.h"

#include <time.h>

void delta_timer_reset(delta_timer_t* timer)
{
	clock_gettime(CLOCK_MONOTONIC_RAW, &timer->start);
	timer->ts = timer->start;
}

void delta_timer_capture(double* deltaTime, double* elapsedTime, delta_timer_t* timer)
{
	struct timespec now;
	struct timespec then;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	
	then = timer->ts;
	timer->ts = now;

	long long delta_ns = (now.tv_sec - then.tv_sec) * 1000000000LL + (now.tv_nsec - then.tv_nsec);
	long long total_ns = (now.tv_sec - timer->start.tv_sec) * 1000000000LL + (now.tv_nsec - timer->start.tv_nsec);

	*deltaTime = delta_ns / 1000000.0;
	*elapsedTime = total_ns / 1000000.0;
}