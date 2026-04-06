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
        /// Requires surface for presentation. Headless not currently supported.
        explicit Device(vk::Instance instance, vk::SurfaceKHR surface, std::span<const char*> deviceExtensions);

    private:
        vk::PhysicalDevice m_PhysicalDevice;
        vk::UniqueDevice m_Device;

        Queue m_GraphicsQueue;
    };
}
