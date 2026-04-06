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
        explicit Device(vk::Instance instance, std::span<const char*> deviceExtensions);

    private:
        vk::PhysicalDevice m_PhysicalDevice;
        vk::UniqueDevice m_Device;

        Queue m_GraphicsQueue;
    };
}
