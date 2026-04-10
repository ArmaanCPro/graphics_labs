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
        vk::SemaphoreCreateInfo semaphoreCI{};

        auto commandPoolsVec = m_Device.createUniqueCommandPools(CommandPoolFlags::ResetCommandBuffer,
            device.graphicsQueue().index, FRAMES_IN_FLIGHT, "FrameCommandPools");

        std::ranges::move(commandPoolsVec, m_CommandPools.begin());

        for (auto i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            m_CommandBuffers[i] = m_Device.allocateCommandBuffer(m_CommandPools[i],
                CommandBufferLevel::Primary, "FrameCommandBuffer" + std::to_string(i));

            m_ImageAvailableSemaphores[i] = vkCheck(device.device().createSemaphoreUnique(semaphoreCI));
            setDebugName(m_Device.device(), *m_ImageAvailableSemaphores[i], "FrameImageAvailableSemaphore" + std::to_string(i));
        }

        for (uint32_t i = 0; i < m_SwapChain.numSwapChainImages(); ++i)
        {
            m_RenderFinishedSemaphores.push_back(vkCheck(device.device().createSemaphoreUnique(semaphoreCI)));
            setDebugName(m_Device.device(), *m_RenderFinishedSemaphores[i], "FrameRenderFinishedSemaphore" + std::to_string(i));
        }

        m_RenderTarget = device.createTexture({ m_SwapChain.swapChainExtent().width, m_SwapChain.swapChainExtent().height, 1 },
            vk::Format::eR16G16B16A16Sfloat,
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment,
            "RenderTarget");
    }

    void Renderer::drawFrame()
    {
        uint64_t waitValue = m_FrameNumber >= FRAMES_IN_FLIGHT ? m_FrameNumber - FRAMES_IN_FLIGHT + 1 : 0;
        std::array timelineSemaphores = { m_Device.timelineSemaphore() };
        std::array<uint64_t, 1> waitValues = { waitValue };
        m_Device.waitSemaphores(timelineSemaphores, waitValues, std::numeric_limits<uint64_t>::max());

        uint32_t swapchainImageIndex = 0;
        vkCheck(m_Device.device().acquireNextImageKHR(m_SwapChain.swapChain(),
            std::numeric_limits<uint64_t>::max(), *m_ImageAvailableSemaphores[m_CurrentFrame],
            nullptr, &swapchainImageIndex));

        CommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

        cmd.reset();

        cmd.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

        float flash = std::abs(std::sin((float)m_FrameNumber / 240.0f));
        vk::ClearColorValue clearValue = vk::ClearColorValue{0.0f, 0.0f, flash, 1.0f};

        cmd.clearColorImage(m_RenderTarget, clearValue, vk::ImageAspectFlagBits::eColor);

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
        cmd.transitionImage(m_SwapChain.swapChainImageHandle(swapchainImageIndex),
            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        cmd.blitImage(m_RenderTarget, m_SwapChain.swapChainImageHandle(swapchainImageIndex));

        cmd.transitionImage(m_SwapChain.swapChainImageHandle(swapchainImageIndex),
            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

        cmd.end();

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
            .commandBuffer = cmd.get(),
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

        std::array<vk::Semaphore, 1> presentWaitSemaphores = { *m_RenderFinishedSemaphores[swapchainImageIndex] };
        m_SwapChain.present(presentWaitSemaphores, swapchainImageIndex,
            m_Device.graphicsQueue().queue);

        m_FrameNumber++;
        m_CurrentFrame = (m_CurrentFrame + 1) % FRAMES_IN_FLIGHT;
    }
}
