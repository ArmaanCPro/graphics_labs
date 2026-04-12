#include "Framing.h"

#include <chrono>
#include <thread>

#include "vulkan/QueueSubmitBuilder.h"

namespace enger
{
    framing::FrameOrchestrator::FrameOrchestrator(Device& device, enger::SwapChain& swapchain, GlfwWindow& window)
        :
        m_Device(device),
        m_GraphicsQueue(device.graphicsQueue()),
        m_Swapchain(swapchain)
    {
        window.setResizeCallback([this](uint32_t width, uint32_t height) { onWindowResize(width, height); });
        
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
            m_RenderFinishedSemaphores.push_back(enger::vkCheck(device.device().createSemaphoreUnique({})));
            enger::setDebugName(device.device(), *m_RenderFinishedSemaphores[i],
                         "FrameRenderFinishedSemaphore" + std::to_string(i));
        }
    }

    framing::FrameOrchestrator::~FrameOrchestrator()
    {
        vkCheck(m_Device.device().waitIdle());
    }

    void framing::FrameOrchestrator::pushLayer(IFrameLayer* layer)
    {
        m_Layers.push_back(layer);
    }

    void framing::FrameOrchestrator::drawFrame()
    {
        if (!m_ShouldRender)
        {
            // throttle
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return;
        }

        m_GraphicsQueue.wait(m_LastFrameSubmits[m_CurrentFrame]);
        m_GraphicsQueue.flushDeletionQueue();

        if (m_ShouldRecreateSwapchain)
        {
            recreateSwapchain();
            //return;
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
            //return;
        }

        enger::CommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
        cmd.reset();
        cmd.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        enger::framing::FrameContext frameContext{
            .cmd = cmd,
            .swapchainImageIndex = swapchainImageIndex,
            .swapchainImageHandle = m_Swapchain.swapChainImageHandle(swapchainImageIndex),
            .swapchainExtent = m_Swapchain.swapChainExtent(),
            .frameIndex = m_CurrentFrame,
        };

        for (auto* layer : m_Layers)
        {
            layer->draw(frameContext);
        }

        // ImGui layer, the final layer, always leaves the swapchain in Color Attachment layout
        cmd.transitionImage(frameContext.swapchainImageHandle,
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

        cmd.end();

        enger::QueueSubmitBuilder submission{};
        submission.waitBinary(*m_ImageAvailableSemaphores[m_CurrentFrame],
                              vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.signalBinary(*m_RenderFinishedSemaphores[swapchainImageIndex],
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.addCmd(cmd);

        m_LastFrameSubmits[m_CurrentFrame] = m_GraphicsQueue.submit(submission.build());
        auto presentResult = m_Swapchain.present(
            {{*m_RenderFinishedSemaphores[swapchainImageIndex]}},
            swapchainImageIndex,
            m_GraphicsQueue
        );

        if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR)
        {
            m_ShouldRecreateSwapchain = true;
        }

        m_CurrentFrame = (m_CurrentFrame + 1) % enger::framing::FRAMES_IN_FLIGHT;
    }

    void framing::FrameOrchestrator::onWindowResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            m_ShouldRender = false;
        }
        else
        {
            m_ShouldRender = true;
        }
        m_ShouldRecreateSwapchain = true;
    }

    void framing::FrameOrchestrator::recreateSwapchain()
    {

    }
}
