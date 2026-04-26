#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <source_location>

#include "Logging/Log.h"

namespace enger
{
    [[noreturn]] inline void vkFatal([[maybe_unused]] vk::Result result, [[maybe_unused]] std::source_location loc = std::source_location::current())
    {
        LOG_ERROR("Vulkan error: {} at {}:{}", vk::to_string(result), loc.file_name(), loc.line());
        std::terminate();
    }

    inline void vkCheck(vk::Result result, std::source_location loc = std::source_location::current())
    {
        if (result != vk::Result::eSuccess) [[unlikely]]
        {
            vkFatal(result, loc);
        }
    }

    template<typename T>
    T vkCheck(vk::ResultValue<T> result, std::source_location loc = std::source_location::current())
    {
        if (result.result != vk::Result::eSuccess) [[unlikely]]
        {
            vkFatal(result.result, loc);
        }
        return std::move(result.value);
    }

#ifdef NDEBUG
    inline constexpr bool kDebugNames = false;
#else
    inline constexpr bool kDebugNames = true;
#endif

    // this is messier, works better with the C types (you can't cast directly from vk:: type to uint64_t)
    inline void setDebugName(
        vk::Device device,
        uint64_t handle,
        vk::ObjectType type,
        std::string_view name
    )
    {
        if constexpr (kDebugNames)
        {
            vk::DebugUtilsObjectNameInfoEXT info{
                .objectType = type,
                .objectHandle = handle,
                .pObjectName = name.data()
            };
            vkCheck(device.setDebugUtilsObjectNameEXT(info));
        }
    }

    template<typename T>
    concept isVulkanHandleType = requires
    {
        typename T::NativeType;
        { T::objectType } -> std::convertible_to<vk::ObjectType>;
    };

    template<typename T>
    requires isVulkanHandleType<T>
    void setDebugName(
        vk::Device device,
        T handle,
        std::string_view name
    )
    {
        if constexpr (kDebugNames)
        {
            vk::DebugUtilsObjectNameInfoEXT info{
                .objectType = T::objectType,
                .objectHandle = static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(static_cast<typename T::NativeType>(handle))),
                .pObjectName = name.data()
            };
            vkCheck(device.setDebugUtilsObjectNameEXT(info));
        }
    }
}
