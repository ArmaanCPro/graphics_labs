#pragma once

#include "cstdint"
#include "vulkan/Device.h"
#include "vulkan/SwapChain.h"

namespace enger
{
    constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    struct FrameData
    {
        vk::UniqueCommandPool _commandPool;
        // the cmdbuf will be destroyed when its parent pool is destroyed, so it doesn't need to be unique
        vk::CommandBuffer _commandBuffer;

        vk::UniqueSemaphore _imageAvailableSemaphore, _renderFinishedSemaphore;
        vk::UniqueFence _inFlightFence;
    };

    class Renderer
    {
    public:
        explicit Renderer(Device& device, SwapChain& swapchain);

        void drawFrame();

    private:
        std::array<FrameData, FRAMES_IN_FLIGHT> m_FrameData;
        Device& m_Device;
        SwapChain& m_SwapChain;

        uint32_t m_CurrentFrame = 0;
        uint32_t m_FrameNumber = 0;
    };
}
