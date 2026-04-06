#pragma once

#include "vk.h"

#include <string_view>

namespace enger
{
    struct Queue
    {
        vk::Queue queue;
        uint32_t index;
    };

    class Device
    {
    public:
        explicit Device(std::span<const char*> instanceExtensions, std::span<const char*> deviceExtensions);

        vk::Instance instance() { return *m_Instance; }

    private:
        /// Must be the first member, as dl owns the instance (vulkan-1.dll) and must outlive all Vulkan handles.
        /// Also, the instance functions are rarely used, as once the dispatcher is initialized with the device, it goes straight to the driver.
        vk::detail::DynamicLoader dl;

        vk::UniqueInstance m_Instance;
        vk::PhysicalDevice m_PhysicalDevice;
        vk::UniqueDevice m_Device;

        Queue m_GraphicsQueue;
    };
}
