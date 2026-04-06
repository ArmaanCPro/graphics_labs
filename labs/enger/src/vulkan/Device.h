#pragma once

#include "vk.h"

#include "Resources.h"
#include "Pipeline.h"

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

        [[nodiscard]] vk::PhysicalDevice physicalDevice() { return m_PhysicalDevice; }
        [[nodiscard]] vk::Device device() { return *m_Device; }

        [[nodiscard]] Queue graphicsQueue() { return m_GraphicsQueue; }

        void destroyComputePipeline(ComputePipelineHandle handle);

    private:
        vk::PhysicalDevice m_PhysicalDevice;
        vk::UniqueDevice m_Device;

        Queue m_GraphicsQueue;

        Pool<ComputePipelineTag, Pipeline> m_ComputePipelinePool;
    };
}
