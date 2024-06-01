#include "delta_time.h"

#include <time.h>

void ResetDeltaTime(delta_timer_t* timer)
{
	clock_gettime(CLOCK_MONOTONIC_RAW, &timer->ts);
}

double CaptureDeltaTime(delta_timer_t* timer)
{
	struct timespec now;
	struct timespec then;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	
	then = timer->ts;
	timer->ts = now;

	long long delta_ns = (now.tv_sec - then.tv_sec) * 1000000000LL + (now.tv_nsec - then.tv_nsec);

	return delta_ns / 1000000.0;
}