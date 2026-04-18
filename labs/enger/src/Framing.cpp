#include "Framing.h"

#include <chrono>
#include <thread>

#include "vulkan/QueueSubmitBuilder.h"

namespace enger::framing
{
    framing::FrameOrchestrator::FrameOrchestrator(Device& device, enger::SwapChain& swapchain, GlfwWindow& window)
        :
        m_Device(device),
        m_GraphicsQueue(device.graphicsQueue()),
        m_Swapchain(swapchain),
        m_Window(window)
    {
        auto commandPoolsVec = device.createUniqueCommandPools(enger::CommandPoolFlags::ResetCommandBuffer,
                                                                 m_GraphicsQueue.familyIndex(), enger::framing::FRAMES_IN_FLIGHT,
                                                                 "FrameCommandPools");

        std::ranges::move(commandPoolsVec, m_CommandPools.begin());

        for (auto i = 0; i < enger::framing::FRAMES_IN_FLIGHT; ++i)
        {
            m_CommandBuffers[i] = device.allocateCommandBuffer(m_CommandPools[i],
                                                                 enger::CommandBufferLevel::Primary,
                                                                 "FrameCommandBuffer" + std::to_string(i));

            m_ImageAvailableSemaphores[i] = enger::vkCheck(device.device().createSemaphoreUnique({}));
            enger::setDebugName(device.device(), *m_ImageAvailableSemaphores[i],
                                "FrameImageAvailableSemaphore" + std::to_string(i));
        }

        m_RenderFinishedSemaphores.reserve(swapchain.numSwapChainImages());
        for (uint32_t i = 0; i < swapchain.numSwapChainImages(); ++i)
        {
            m_RenderFinishedSemaphores.push_back(vkCheck(device.device().createSemaphoreUnique({})));
            enger::setDebugName(device.device(), *m_RenderFinishedSemaphores[i],
                         "FrameRenderFinishedSemaphore" + std::to_string(i));
        }
    }

    framing::FrameOrchestrator::~FrameOrchestrator()
    {
        vkCheck(m_Device.device().waitIdle());
    }

    std::expected<framing::FrameContext, FrameOrchestrator::BeginFrameError> framing::FrameOrchestrator::beginFrame()
    {
        if (m_LastFrameSubmits[m_CurrentFrame].timelineSemaphore != nullptr) [[likely]]
            m_GraphicsQueue.wait(m_LastFrameSubmits[m_CurrentFrame]);
        m_GraphicsQueue.flushDeletionQueue();

        if (m_ShouldRecreateSwapchain)
        {
            recreateSwapchain();
            return std::unexpected{BeginFrameError::SwapchainRecreateRequired};
        }

        uint32_t swapchainImageIndex = 0;
        auto acquireResult = m_Device.device().acquireNextImageKHR(
            m_Swapchain.swapChain(),
            std::numeric_limits<uint64_t>::max(),
            *m_ImageAvailableSemaphores[m_CurrentFrame],
            nullptr, &swapchainImageIndex
        );

        if (acquireResult == vk::Result::eErrorOutOfDateKHR)
        {
            m_ShouldRecreateSwapchain = true;
            return std::unexpected{BeginFrameError::SwapchainRecreateRequired};
        }

        enger::CommandBuffer& cmd = m_CommandBuffers[m_CurrentFrame];
        cmd.reset();
        cmd.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        return FrameContext{
            .cmd = cmd,
            .swapchainImageIndex = swapchainImageIndex,
            .swapchainImageHandle = m_Swapchain.swapChainImageHandle(swapchainImageIndex),
            .swapchainExtent = m_Swapchain.swapChainExtent(),
            .frameIndex = m_CurrentFrame
        };
    }

    void framing::FrameOrchestrator::endFrame(FrameContext& fctx)
    {
        ENGER_PROFILE_FUNCTION();
        auto* device = &m_Device;
        ENGER_PROFILE_GPU_COLLECT(device, fctx.cmd.get());

        auto& cmd = fctx.cmd;

        // ImGui layer, the final layer, always leaves the swapchain in Color Attachment layout
        cmd.transitionImage(fctx.swapchainImageHandle,
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

        cmd.end();

        enger::QueueSubmitBuilder submission{};
        submission.waitBinary(*m_ImageAvailableSemaphores[fctx.frameIndex],
                              vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.signalBinary(*m_RenderFinishedSemaphores[fctx.swapchainImageIndex],
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.addCmd(cmd);

        for (const auto& wait : fctx.desiredWaits)
        {
            submission.waitTimeline(wait, vk::PipelineStageFlagBits2::eAllCommands);
        }

        m_LastFrameSubmits[fctx.frameIndex] = m_GraphicsQueue.submit(submission.build());
        auto presentResult = m_Swapchain.present(
            {{*m_RenderFinishedSemaphores[fctx.swapchainImageIndex]}},
            fctx.swapchainImageIndex,
            m_GraphicsQueue
        );

        if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR)
        {
            m_ShouldRecreateSwapchain = true;
        }

        m_CurrentFrame = (m_CurrentFrame + 1) % FRAMES_IN_FLIGHT;

        ENGER_PROFILE_FRAME("Frame");
    }

    void framing::FrameOrchestrator::onWindowResize([[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height)
    {
        m_ShouldRecreateSwapchain = true;
    }

    void framing::FrameOrchestrator::recreateSwapchain()
    {
        auto [width, height] = m_Window.framebufferSize();

        m_GraphicsQueue.waitIdle();
        m_Device.waitIdle();

        m_Swapchain.recreate(width, height);

        m_RenderFinishedSemaphores.clear();
        m_RenderFinishedSemaphores.reserve(m_Swapchain.numSwapChainImages());
        for (uint32_t i = 0; i < m_Swapchain.numSwapChainImages(); ++i)
        {
            m_RenderFinishedSemaphores.push_back(enger::vkCheck(m_Device.device().createSemaphoreUnique({})));
            enger::setDebugName(m_Device.device(), *m_RenderFinishedSemaphores[i],
                         "FrameRenderFinishedSemaphore" + std::to_string(i));
        }

        m_ShouldRecreateSwapchain = false;
    }
}
