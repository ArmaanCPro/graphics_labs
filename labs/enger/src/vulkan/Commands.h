#pragma once

#include "vk.h"

#include "Resources.h"

namespace enger
{
    class Device;

    enum class CommandPoolFlags
    {
        Transient,
        ResetCommandBuffer,
    };

    struct UniqueCommandPool
    {
        vk::UniqueCommandPool m_CommandPool;
        uint32_t m_QueueFamilyIndex;
    };

    enum class CommandBufferLevel
    {
        Primary,
        //Secondary,
    };

    // TODO improve the overall API & impl. Remove friend classes
    class CommandBuffer
    {
        friend class Device;
    public:
        CommandBuffer() = default;
        // temporary
        vk::CommandBuffer& get() { return m_CommandBuffer; }

        void begin(vk::CommandBufferUsageFlags flags);
        void end();
        void reset();
        void transitionImage(TextureHandle texHandle, vk::ImageLayout srcLayout, vk::ImageLayout dstLayout);
        void blitImage(TextureHandle srcTexHandle, TextureHandle dstTexHandle);
        void clearColorImage(TextureHandle texHandle, vk::ClearColorValue color, vk::ImageAspectFlags aspectMask);

    private:
        CommandBuffer(Device* device, vk::CommandBuffer commandBuffer);
        Device* m_Device;
        vk::CommandBuffer m_CommandBuffer;
    };
}
