#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include "vulkan/Device.h"
#include "vulkan/SwapChain.h"

namespace enger
{
    constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    class Renderer
    {
    public:
        explicit Renderer(Device& device, SwapChain& swapchain);

        void drawFrame();

    private:
        Device& m_Device;
        SwapChain& m_SwapChain;

        uint32_t m_CurrentFrame = 0;
        uint32_t m_FrameNumber = 0;

        std::array<vk::UniqueCommandPool, FRAMES_IN_FLIGHT> m_CommandPools;
        // the cmdbuf will be destroyed when its parent pool is destroyed, so it doesn't need to be unique
        std::array<vk::CommandBuffer, FRAMES_IN_FLIGHT> m_CommandBuffers;

        std::array<vk::UniqueSemaphore, FRAMES_IN_FLIGHT> m_ImageAvailableSemaphores;
        std::vector<vk::UniqueSemaphore> m_RenderFinishedSemaphores;
        std::array<vk::UniqueFence, FRAMES_IN_FLIGHT> m_InFlightFences;
    };
}
