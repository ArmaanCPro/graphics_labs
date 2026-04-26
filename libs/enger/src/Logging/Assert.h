#pragma once

#include <Logging/Log.h>

namespace enger
{

#ifndef NDEBUG

    #if defined(_WIN32)
        #define E_HALT() __debugbreak()
    #elif defined(__clang__)
        #define E_HALT() __builtin_debugtrap()
    #elif defined(__GNUC__)
        #define E_HALT() __builtin_trap()
    #else
        #warning "Unsupported compiler for E_HALT macro"
    #endif

#endif


#ifndef NDEBUG

#define EASSERT(cond, ...) \
    do { \
        if (!(cond)) { \
            LOG_FATAL("[Assertion Failed] ({}), {}:{},\n\t" __VA_OPT__(__VA_ARGS__), #cond, __FILE__, __LINE__); \
            E_HALT(); \
        } \
    } while (0)

#else

#define EASSERT(cond, ...) \
    do { \
        if (!(cond)) { \
            LOG_FATAL("[Assertion Failed] ({}), {}:{},\n\t" __VA_OPT__(__VA_ARGS__), #cond, __FILE__, __LINE__); \
        } \
    } while (0)

#endif
}
