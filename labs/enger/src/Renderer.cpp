#include "Renderer.h"

namespace enger
{
    void transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout srcLayout, vk::ImageLayout dstLayout)
    {
        vk::ImageMemoryBarrier2 barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            .oldLayout = srcLayout,
            .newLayout = dstLayout,
            .image = image,
            .subresourceRange = vk::ImageSubresourceRange{
                .aspectMask = dstLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
            }
        };

        vk::DependencyInfo dependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier,
        };

        cmd.pipelineBarrier2(dependencyInfo);
    }

    Renderer::Renderer(Device &device, SwapChain& swapchain)
        : m_Device(device), m_SwapChain(swapchain)
    {
        vk::CommandPoolCreateInfo commandPoolCI{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = device.graphicsQueue().index,
        };

        vk::FenceCreateInfo fenceCI{
            .flags = vk::FenceCreateFlagBits::eSignaled,
        };

        vk::SemaphoreCreateInfo semaphoreCI{};

        for (auto i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            m_FrameData[i]._commandPool = vkCheck(device.device().createCommandPoolUnique(commandPoolCI));

            vk::CommandBufferAllocateInfo cmdAllocCI{
                .commandPool = *m_FrameData[i]._commandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };

            m_FrameData[i]._commandBuffer = vkCheck(device.device().allocateCommandBuffers(cmdAllocCI)).front();

            m_FrameData[i]._inFlightFence = vkCheck(device.device().createFenceUnique(fenceCI));
            m_FrameData[i]._imageAvailableSemaphore = vkCheck(device.device().createSemaphoreUnique(semaphoreCI));
            m_FrameData[i]._renderFinishedSemaphore = vkCheck(device.device().createSemaphoreUnique(semaphoreCI));
        }
    }

    void Renderer::drawFrame()
    {
        vkCheck(m_Device.device().waitForFences(1, &*m_FrameData[m_CurrentFrame]._inFlightFence, true, std::numeric_limits<uint64_t>::max()));
        vkCheck(m_Device.device().resetFences(1, &*m_FrameData[m_CurrentFrame]._inFlightFence));

        uint32_t swapchainImageIndex = 0;
        vkCheck(m_Device.device().acquireNextImageKHR(m_SwapChain.swapChain(),
            std::numeric_limits<uint64_t>::max(), m_FrameData[m_CurrentFrame]._imageAvailableSemaphore.get(),
            nullptr, &swapchainImageIndex));

        vk::CommandBuffer cmd = m_FrameData[m_CurrentFrame]._commandBuffer;

        vkCheck(cmd.reset());

        vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        };

        vkCheck(cmd.begin(beginInfo));

        transitionImage(cmd, m_SwapChain.swapChainImage(swapchainImageIndex), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

        vk::ImageSubresourceRange clearRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        float flash = std::abs(std::sin(m_FrameNumber / 240.0f));
        vk::ClearColorValue clearValue = vk::ClearColorValue{0.0f, 0.0f, flash, 1.0f};

        cmd.clearColorImage(m_SwapChain.swapChainImage(swapchainImageIndex), vk::ImageLayout::eGeneral, &clearValue, 1, &clearRange);

        transitionImage(cmd, m_SwapChain.swapChainImage(swapchainImageIndex), vk::ImageLayout::eGeneral, vk::ImageLayout::ePresentSrcKHR);

        vkCheck(cmd.end());

        vk::SemaphoreSubmitInfo waitInfo{
            .semaphore = m_FrameData[m_CurrentFrame]._imageAvailableSemaphore.get(),
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        };

        vk::SemaphoreSubmitInfo signalInfo{
            .semaphore = m_FrameData[m_CurrentFrame]._renderFinishedSemaphore.get(),
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        };

        vk::CommandBufferSubmitInfo cmdSubmitInfo{
            .commandBuffer = cmd,
        };

        vk::SubmitInfo2 submitInfo{
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdSubmitInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalInfo,
        };

        vkCheck(m_Device.graphicsQueue().queue.submit2(1, &submitInfo, m_FrameData[m_CurrentFrame]._inFlightFence.get()));

        auto swapchain = m_SwapChain.swapChain();
        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_FrameData[m_CurrentFrame]._renderFinishedSemaphore.get(),
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &swapchainImageIndex,
        };

        vkCheck(m_Device.graphicsQueue().queue.presentKHR(presentInfo));

        m_FrameNumber++;
        m_CurrentFrame = (m_CurrentFrame + 1) % FRAMES_IN_FLIGHT;
    }
}
