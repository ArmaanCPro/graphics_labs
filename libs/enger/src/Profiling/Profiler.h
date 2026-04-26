#pragma once

#ifdef ENABLE_PROFILING
#include <tracy/Tracy.hpp>
#endif

namespace enger
{
inline constexpr auto ENGER_PROFILE_COLOR_WAIT    = 0xff0000u;
inline constexpr auto ENGER_PROFILE_COLOR_SUBMIT  = 0x0000ffu;
inline constexpr auto ENGER_PROFILE_COLOR_PRESENT = 0x00ff00u;
inline constexpr auto ENGER_PROFILE_COLOR_CREATE  = 0xff6600u;
inline constexpr auto ENGER_PROFILE_COLOR_DESTROY = 0xffa500u;
inline constexpr auto ENGER_PROFILE_COLOR_BARRIER = 0xffffffu;
inline constexpr auto ENGER_PROFILE_COLOR_DRAW    = 0x00ffffu;

#ifdef ENABLE_PROFILING

#define ENGER_PROFILE_FUNCTION() ZoneScoped; (void)___tracy_scoped_zone;
#define ENGER_PROFILE_FUNCTION_COLOR(color) ZoneScopedC(color); (void)___tracy_scoped_zone;
#define ENGER_PROFILE_ZONENC(name, color) \
    ZoneScopedNC(name, color); (void)___tracy_scoped_zone;
#define ENGER_PROFILE_ZONEN(name) \
    ZoneScopedN(name); (void)___tracy_scoped_zone;
#define ENGER_PROFILE_ZONE() \
    ZoneScoped; (void)___tracy_scoped_zone;

#define ENGER_PROFILE_THREAD(name) tracy::SetThreadName(name);
#define ENGER_PROFILE_FRAMEN(name) FrameMarkNamed(name);
#define ENGER_PROFILE_FRAME() FrameMark;

#else

#define ENGER_PROFILE_FUNCTION()
#define ENGER_PROFILE_FUNCTION_COLOR(color)
#define ENGER_PROFILE_ZONENC(name, color)
#define ENGER_PROFILE_ZONEN(name)
#define ENGER_PROFILE_ZONE()
#define ENGER_PROFILE_THREAD(name)
#define ENGER_PROFILE_FRAMEN(name)
#define ENGER_PROFILE_FRAME()

#endif
}
