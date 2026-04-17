#pragma once

#ifdef ENABLE_PROFILING
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#endif

namespace enger
{
inline constexpr uint32_t ENGER_PROFILER_COLOR_WAIT    = 0xff0000;
inline constexpr uint32_t ENGER_PROFILER_COLOR_SUBMIT  = 0x0000ff;
inline constexpr uint32_t ENGER_PROFILER_COLOR_PRESENT = 0x00ff00;
inline constexpr uint32_t ENGER_PROFILER_COLOR_CREATE  = 0xff6600;
inline constexpr uint32_t ENGER_PROFILER_COLOR_DESTROY = 0xffa500;
inline constexpr uint32_t ENGER_PROFILER_COLOR_BARRIER = 0xffffff;

#ifdef ENABLE_PROFILING

#define ENGER_PROFILE_FUNCTION() ZoneScoped
#define ENGER_PROFILE_FUNCTION_COLOR(color) ZoneScopedC(color)
#define ENGER_PROFILE_ZONE(name, color) \
    ZoneScopedNC(name, color);

#define ENGER_PROFILE_THREAD(name) tracy::SetThreadName(name)
#define ENGER_PROFILE_FRAME(name) FrameMarkNamed(name)

#define ENGER_PROFILE_GPU_ZONE(name, device, cmdBuffer, color) \
    TracyVkZoneC(device->m_TracyVkCtx, cmdBuffer, name, color)

    // TODO consider moving the GPU stuff into the Device class file, and then replacing this with an inline function
#define ENGER_PROFILE_GPU_COLLECT(device, cmdBuffer) \
    do { \
        if (device->m_UsingTracyHostCalibrated) { \
            TracyVkCollectHost(device->m_TracyVkCtx); }\
        else { \
            TracyVkCollect(device->m_TracyVkCtx, cmdBuffer); }\
    } while (0)

#else

#define ENGER_PROFILE_FUNCTION()
#define ENGER_PROFILE_FUNCTION_COLOR(color)
#define ENGER_PROFILE_ZONE(name, color)
#define ENGER_PROFILE_THREAD(name)
#define ENGER_PROFILE_FRAME(name)

#define ENGER_PROFILE_GPU_ZONE(name, device, cmdBuffer, color)

#define ENGER_PROFILE_GPU_COLLECT(device, cmdBuffer)

#endif
}
