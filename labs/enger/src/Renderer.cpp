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
                .levelCount = vk::RemainingMipLevels,
                .baseArrayLayer = 0,
                .layerCount = vk::RemainingArrayLayers,
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

        vk::SemaphoreCreateInfo semaphoreCI{};

        for (auto i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            m_CommandPools[i] = vkCheck(device.device().createCommandPoolUnique(commandPoolCI));
            setDebugName(m_Device.device(), *m_CommandPools[i], "FrameCommandPool" + std::to_string(i));

            vk::CommandBufferAllocateInfo cmdAllocCI{
                .commandPool = *m_CommandPools[i],
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };

            m_CommandBuffers[i] = vkCheck(device.device().allocateCommandBuffers(cmdAllocCI)).front();
            setDebugName(m_Device.device(), m_CommandBuffers[i], "FrameCommandBuffer" + std::to_string(i));

            m_ImageAvailableSemaphores[i] = vkCheck(device.device().createSemaphoreUnique(semaphoreCI));
            setDebugName(m_Device.device(), *m_ImageAvailableSemaphores[i], "FrameImageAvailableSemaphore" + std::to_string(i));

        }

        for (uint32_t i = 0; i < m_SwapChain.numSwapChainImages(); ++i)
        {
            m_RenderFinishedSemaphores.push_back(vkCheck(device.device().createSemaphoreUnique(semaphoreCI)));
            setDebugName(m_Device.device(), *m_RenderFinishedSemaphores[i], "FrameRenderFinishedSemaphore" + std::to_string(i));
        }
    }

    void Renderer::drawFrame()
    {
        uint64_t waitValue = m_FrameNumber >= FRAMES_IN_FLIGHT ? m_FrameNumber - FRAMES_IN_FLIGHT + 1 : 0;
        auto timelineSemaphore = m_Device.timelineSemaphore();
        vk::SemaphoreWaitInfo waitInfo{
            .semaphoreCount = 1,
            .pSemaphores = &timelineSemaphore,
            .pValues = &waitValue,
        };
        vkCheck(m_Device.device().waitSemaphores(waitInfo, std::numeric_limits<uint64_t>::max()));

        uint32_t swapchainImageIndex = 0;
        vkCheck(m_Device.device().acquireNextImageKHR(m_SwapChain.swapChain(),
            std::numeric_limits<uint64_t>::max(), *m_ImageAvailableSemaphores[m_CurrentFrame],
            nullptr, &swapchainImageIndex));

        vk::CommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

        vkCheck(cmd.reset());

        vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        };

        vkCheck(cmd.begin(beginInfo));

        transitionImage(cmd, m_SwapChain.swapChainImage(swapchainImageIndex), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

        vk::ImageSubresourceRange clearRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = vk::RemainingMipLevels,
            .baseArrayLayer = 0,
            .layerCount = vk::RemainingArrayLayers,
        };

        float flash = std::abs(std::sin((float)m_FrameNumber / 240.0f));
        vk::ClearColorValue clearValue = vk::ClearColorValue{0.0f, 0.0f, flash, 1.0f};

        cmd.clearColorImage(m_SwapChain.swapChainImage(swapchainImageIndex), vk::ImageLayout::eGeneral, &clearValue, 1, &clearRange);

        transitionImage(cmd, m_SwapChain.swapChainImage(swapchainImageIndex), vk::ImageLayout::eGeneral, vk::ImageLayout::ePresentSrcKHR);

        vkCheck(cmd.end());

        vk::SemaphoreSubmitInfo submitWaitInfo{
            .semaphore = *m_ImageAvailableSemaphores[m_CurrentFrame],
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        };

        std::array<vk::SemaphoreSubmitInfo, 2> submitSignalInfo = {
            vk::SemaphoreSubmitInfo{
                .semaphore = *m_RenderFinishedSemaphores[swapchainImageIndex],
                .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            },
            vk::SemaphoreSubmitInfo{
                .semaphore = timelineSemaphore,
                .value = m_FrameNumber + 1,
                .stageMask = vk::PipelineStageFlagBits2::eAllGraphics,
            },
        };

        vk::CommandBufferSubmitInfo cmdSubmitInfo{
            .commandBuffer = cmd,
        };

        vk::SubmitInfo2 submitInfo{
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &submitWaitInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdSubmitInfo,
            .signalSemaphoreInfoCount = static_cast<uint32_t>(submitSignalInfo.size()),
            .pSignalSemaphoreInfos = submitSignalInfo.data(),
        };

        m_Device.submitGraphics(submitInfo);

        auto swapchain = m_SwapChain.swapChain();
        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*m_RenderFinishedSemaphores[swapchainImageIndex],
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &swapchainImageIndex,
        };

        vkCheck(m_Device.graphicsQueue().queue.presentKHR(presentInfo));

        m_FrameNumber++;
        m_CurrentFrame = (m_CurrentFrame + 1) % FRAMES_IN_FLIGHT;
    }
}
