#pragma once


/*
 * Moved these defines to the build system
#define VK_NO_PROTOTYPES
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
*/

#include <vulkan/vulkan.hpp>

#include <source_location>
#include <iostream>

namespace enger
{
    [[noreturn]] inline void vkFatal(vk::Result result, std::source_location loc = std::source_location::current())
    {
        std::cerr << "Vulkan error: " << vk::to_string(result) << " at " << loc.file_name() << ":" << loc.line() << std::endl;
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
    T vkCheck(vk::ResultValue<T> result)
    {
        if (result.result != vk::Result::eSuccess) [[unlikely]]
        {
            vkFatal(result.result);
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
