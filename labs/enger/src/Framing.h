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

    /// Describes a layer in the frame rendering pipeline.
    /// Honestly, not sure how great this is.
    /// It's not a full layer-stack system, this is solely for decoupling renderers that draw
    /// (for now) sequentially in a frame.
    class IFrameLayer
    {
    public:
        virtual ~IFrameLayer() = default;

        virtual void draw(FrameContext& ctx) = 0;

        virtual void onResize([[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height) {};
    };

    /// This exists for the sake of decoupling the core Renderer & ImGui.
    /// This orchestrator manages SOLELY frame rendering, not any CPU/game logic.
    /// Currently, the Frame Orchestrator only allows single-Queue operations.
    /// A better solution would be a full Frame Graph.
    class FrameOrchestrator
    {
    public:
        FrameOrchestrator(Device& device, enger::SwapChain& swapchain, GlfwWindow& window);
        ~FrameOrchestrator();

        void pushLayer(IFrameLayer* layer);

        void drawFrame();

    private:
        void onWindowResize(uint32_t width, uint32_t height);

        void recreateSwapchain();

    private:
        Device& m_Device;
        Queue& m_GraphicsQueue;
        SwapChain& m_Swapchain;

        std::vector<IFrameLayer*> m_Layers;

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
