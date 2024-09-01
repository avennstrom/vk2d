#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void profilerFrameMark(void);
void profilerZoneBegin(const char* name);
void profilerZoneEnd(void);

#ifdef __cplusplus
}
#endif

#define PROFILER_FRAME_MARK()	profilerFrameMark()
#define PROFILER_BEGIN(Name)	profilerZoneBegin(#Name)
#define PROFILER_END()			profilerZoneEnd()