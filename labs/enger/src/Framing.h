#pragma once

#include <cstdint>
#include "vulkan/Commands.h"
#include "vulkan/SwapChain.h"

namespace enger::framing
{
    static constexpr auto FRAMES_IN_FLIGHT = 2;

    // Used by renderers per frame. Contains information pertinent to rendering.
    struct FrameContext
    {
        CommandBuffer& cmd;
        uint32_t swapchainImageIndex;
        TextureHandle swapchainImageHandle;
        vk::Extent2D swapchainExtent;
        uint32_t frameIndex;
    };

    /// The FrameOrchestrator simply manages frame synchronization AND swapchain recreation/resizing.
    /// It is not tightly coupled to renderers and is meant to be used as a standalone orchestrator.
    /// The proper usage is to put any graphics logic within the begin/endFrame() functions.
    class FrameOrchestrator
    {
    public:
        FrameOrchestrator(Device& device, enger::SwapChain& swapchain, GlfwWindow& window);
        ~FrameOrchestrator();

        std::optional<FrameContext> beginFrame();
        void endFrame(FrameContext& fctx);

    private:
        void onWindowResize(uint32_t width, uint32_t height);

        void recreateSwapchain();

    private:
        Device& m_Device;
        Queue& m_GraphicsQueue;
        SwapChain& m_Swapchain;
        GlfwWindow& m_Window;

        std::array<enger::SubmitHandle, enger::framing::FRAMES_IN_FLIGHT> m_LastFrameSubmits = {0, 0};
        std::array<enger::UniqueCommandPool, enger::framing::FRAMES_IN_FLIGHT> m_CommandPools;
        std::array<enger::CommandBuffer, enger::framing::FRAMES_IN_FLIGHT> m_CommandBuffers;

        std::array<vk::UniqueSemaphore, enger::framing::FRAMES_IN_FLIGHT> m_ImageAvailableSemaphores;
        std::vector<vk::UniqueSemaphore> m_RenderFinishedSemaphores;

        uint32_t m_CurrentFrame = 0;

        bool m_ShouldRender = true;

        bool m_ShouldRecreateSwapchain = false;
    };
}
