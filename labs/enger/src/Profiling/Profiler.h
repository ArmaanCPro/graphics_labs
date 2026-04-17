#pragma once

#ifdef ENABLE_PROFILING
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#endif

namespace enger
{
#define ENGER_PROFILER_COLOR_WAIT 0xff0000
#define ENGER_PROFILER_COLOR_SUBMIT 0x0000ff
#define ENGER_PROFILER_COLOR_PRESENT 0x00ff00
#define ENGER_PROFILER_COLOR_CREATE 0xff6600
#define ENGER_PROFILER_COLOR_DESTROY 0xffa500
#define ENGER_PROFILER_COLOR_BARRIER 0xffffff

#ifdef ENABLE_PROFILING

#define ENGER_PROFILE_FUNCTION() ZoneScoped
#define ENGER_PROFILE_FUNCTION_COLOR(color) ZoneScopedC(color)
#define ENGER_PROFILE_ZONE(name, color) { \
    ZoneScopedC(color); \
    ZoneName(name, strlen(name))
#define ENGER_PROFILE_ZONE_END() }

#define ENGER_PROFILE_THREAD(name) tracy::SetThreadName(name)
#define ENGER_PROFILE_FRAME(name) FrameMarkNamed(name)

#define ENGER_PROFILE_GPU_ZONE(name, device, cmdBuffer, color) \
    TracyVkZoneC(device.m_TracyVkCtx, cmdBuffer, name, color)

#else

#define ENGER_PROFILE_FUNCTION()
#define ENGER_PROFILE_FUNCTION_COLOR(color)
#define ENGER_PROFILE_ZONE(name, color) {
#define ENGER_PROFILE_ZONE_END() }
#define ENGER_PROFILE_THREAD(name)
#define ENGER_PROFILE_FRAME(name)

#define ENGER_PROFILE_GPU_ZONE(name, device, cmdBuffer, color)

#endif
}
