#pragma once

#include <expected>

#include "vulkan/Commands.h"
#include "vulkan/SwapChain.h"

namespace enger::framing
{
    static constexpr auto FRAMES_IN_FLIGHT = 2;

    // Used by renderers per frame. Contains information pertinent to rendering.
    struct ENGER_EXPORT FrameContext
    {
        CommandBuffer& cmd;
        uint32_t swapchainImageIndex;
        TextureHandle swapchainImageHandle;
        vk::Extent2D swapchainExtent;
        uint32_t frameIndex;
        std::vector<SubmitHandle> desiredWaits;
    };

    /// The FrameOrchestrator simply manages frame synchronization AND swapchain recreation/resizing.
    /// It is not tightly coupled to renderers and is meant to be used as a standalone orchestrator.
    /// The proper usage is to put any graphics logic within the begin/endFrame() functions.
    class ENGER_EXPORT FrameOrchestrator
    {
    public:
        FrameOrchestrator(Device& device, SwapChain& swapchain, GlfwWindow& window);
        ~FrameOrchestrator();

        enum class BeginFrameError
        {
            SwapchainRecreateRequired,
            Other
        };
        std::expected<FrameContext, BeginFrameError> beginFrame();
        void endFrame(FrameContext& fctx);

        void onWindowResize(uint32_t width, uint32_t height);
    private:

        void recreateSwapchain();

    private:
        Device& m_Device;
        Queue& m_GraphicsQueue;
        SwapChain& m_Swapchain;
        GlfwWindow& m_Window;

        std::array<SubmitHandle, FRAMES_IN_FLIGHT> m_LastFrameSubmits;
        std::array<UniqueCommandPool, FRAMES_IN_FLIGHT> m_CommandPools;
        std::array<CommandBuffer, FRAMES_IN_FLIGHT> m_CommandBuffers;

        std::array<vk::UniqueSemaphore, FRAMES_IN_FLIGHT> m_ImageAvailableSemaphores;
        std::vector<vk::UniqueSemaphore> m_RenderFinishedSemaphores;

        uint32_t m_CurrentFrame = 0;

        bool m_ShouldRender = true;

        bool m_ShouldRecreateSwapchain = false;
    };
}
