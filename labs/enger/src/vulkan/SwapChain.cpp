#include "SwapChain.h"

namespace enger
{
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        for (const auto& format: availableFormats)
        {
            if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return format;
            }
        }
        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(vk::PresentModeKHR desiredMode,
                                             const std::vector<vk::PresentModeKHR>& availablePresentModes)
    {
        for (const auto& presentMode: availablePresentModes)
        {
            if (presentMode == desiredMode)
            {
                return presentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& caps, uint32_t width, uint32_t height)
    {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return caps.currentExtent;
        }

        return vk::Extent2D{
            std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }

    uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& caps)
    {
        auto minImageCount = std::max(3u, caps.minImageCount);
        // 0 means there is no max
        if ((0 < caps.maxImageCount) && (minImageCount > caps.maxImageCount))
        {
            minImageCount = caps.maxImageCount;
        }
        return minImageCount;
    }

    SwapChain::SwapChain(Device& device, vk::SurfaceKHR surface, const GlfwWindow& window,
                         vk::PresentModeKHR desiredPresentMode)
        :
        m_Device(device),
        m_Surface(surface)
    {
        createSwapChain(window.framebufferSize().first, window.framebufferSize().second, desiredPresentMode);
    }

    SwapChain::~SwapChain()
    {
        destroySwapchainHandles();
    }

    void SwapChain::recreate(uint32_t width, uint32_t height, std::optional<vk::PresentModeKHR> desiredPresentMode)
    {
        vk::SwapchainKHR oldSwapchain = *m_SwapChain;
        destroySwapchainHandles();
        createSwapChain(width, height, desiredPresentMode.value_or(m_CurrentPresentMode), oldSwapchain);
    }

    vk::Result SwapChain::present(std::span<const vk::Semaphore> waitSemaphores, uint32_t imageIndex, Queue& queue)
    {
        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
            .pWaitSemaphores = waitSemaphores.data(),
            .swapchainCount = 1,
            .pSwapchains = &*m_SwapChain,
            .pImageIndices = &imageIndex,
        };
        return queue.queue().presentKHR(presentInfo);
    }

    void SwapChain::createSwapChain(uint32_t width, uint32_t height, vk::PresentModeKHR desiredPresentMode,
                                    std::optional<vk::SwapchainKHR> oldSwapchain)
    {
        auto physicalDevice = m_Device.physicalDevice();
        auto logicalDevice = m_Device.device();

        auto surfaceCaps = vkCheck(physicalDevice.getSurfaceCapabilitiesKHR(m_Surface));

        m_SwapExtent = chooseSwapExtent(surfaceCaps, width, height);
        uint32_t minImageCount = chooseSwapMinImageCount(surfaceCaps);

        std::vector<vk::SurfaceFormatKHR> availableFormats = vkCheck(physicalDevice.getSurfaceFormatsKHR(m_Surface));
        m_SwapFormat = chooseSwapSurfaceFormat(availableFormats);

        const vk::SurfaceFormatKHR surfaceFormat =
            chooseSwapSurfaceFormat(vkCheck(physicalDevice.getSurfaceFormatsKHR(m_Surface)));

        std::vector<vk::PresentModeKHR> availablePresentModes = vkCheck(
            physicalDevice.getSurfacePresentModesKHR(m_Surface));

        m_CurrentPresentMode = chooseSwapPresentMode(desiredPresentMode, availablePresentModes);

        vk::SwapchainCreateInfoKHR swapchainCI{
            .surface = m_Surface,
            .minImageCount = minImageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = m_SwapExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
            // TODO need to check that it actually supports eTransferDst
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCaps.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = m_CurrentPresentMode,
            .clipped = vk::True,
            .oldSwapchain = oldSwapchain.value_or(nullptr),
        };

        m_SwapChain = vkCheck(logicalDevice.createSwapchainKHRUnique(swapchainCI));
        setDebugName(logicalDevice, *m_SwapChain, "SwapChain");
        m_SwapChainImages = vkCheck(logicalDevice.getSwapchainImagesKHR(*m_SwapChain));
        for (size_t i = 0; i < m_SwapChainImages.size(); ++i)
        {
            setDebugName(logicalDevice, m_SwapChainImages[i], "SwapChainImage" + std::to_string(i));
        }

        vk::ImageViewCreateInfo viewCI{
            .viewType = vk::ImageViewType::e2D,
            .format = surfaceFormat.format,
            .subresourceRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
        };

        m_SwapChainImageViews.reserve(m_SwapChainImages.size());
        for (uint32_t i = 0; i < m_SwapChainImages.size(); ++i)
        {
            viewCI.image = m_SwapChainImages[i];
            m_SwapChainImageViews.push_back(std::move(vkCheck(logicalDevice.createImageViewUnique(viewCI))));
            setDebugName(logicalDevice, *m_SwapChainImageViews[i], "SwapChainImageView" + std::to_string(i));
        }

        for (uint32_t i = 0; i < m_SwapChainImages.size(); ++i)
        {
            VulkanImage img{
                .image_ = m_SwapChainImages[i],
                .view_ = *m_SwapChainImageViews[i],
                .extent_ = {m_SwapExtent.width, m_SwapExtent.height, 1},
                .usage_ = vk::ImageUsageFlagBits::eColorAttachment,
                .format_ = surfaceFormat.format,
            };
            m_SwapchainImageHandles.push_back(m_Device.addTextureToPool(std::move(img)));
        }
    }

    void SwapChain::destroySwapchainHandles()
    {
        for (auto i = 0; i < m_SwapchainImageHandles.size(); ++i)
        {
            assert(m_SwapchainImageHandles[i]);
            m_Device.removeTextureFromPool(m_SwapchainImageHandles[i]);
        }

        m_SwapchainImageHandles.clear();
        m_SwapChainImageViews.clear();
        m_SwapChainImages.clear();
    }
}
