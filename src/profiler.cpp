#include "profiler.h"
#include "util.h"

#include "tracy/Tracy.hpp"

#include <assert.h>

extern "C" void profilerFrameMark(void)
{
	FrameMark;
}

static tracy::ScopedZone*	g_zones[64];
static size_t				g_zoneTop = 0;

extern "C" void profilerZoneBegin(const char* name)
{
	assert(g_zoneTop < countof(g_zones));
	g_zones[g_zoneTop++] = new tracy::ScopedZone(0, "yourmom.c", 9, "asdf()", 6, name, strlen(name), 0x20202020u);
}

extern "C" void profilerZoneEnd(void)
{
	delete g_zones[--g_zoneTop];
}