#include "SwapChain.h"

namespace enger
{
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        for (const auto& format : availableFormats)
        {
            if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return format;
            }
        }
        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(vk::PresentModeKHR desiredMode, const std::vector<vk::PresentModeKHR>& availablePresentModes)
    {
        for (const auto& presentMode : availablePresentModes)
        {
            if (presentMode == desiredMode)
            {
                return presentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& caps, GLFWwindow* window)
    {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return caps.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        return vk::Extent2D{
            std::clamp<uint32_t>(width, caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp<uint32_t>(height, caps.minImageExtent.height, caps.maxImageExtent.height)
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

    SwapChain::SwapChain(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface, GLFWwindow* window, vk::PresentModeKHR desiredPresentMode)
    {
        auto surfaceCaps = vkCheck(physicalDevice.getSurfaceCapabilitiesKHR(surface));

        m_SwapExtent = chooseSwapExtent(surfaceCaps, window);
        uint32_t minImageCount = chooseSwapMinImageCount(surfaceCaps);

        std::vector<vk::SurfaceFormatKHR> availableFormats = vkCheck(physicalDevice.getSurfaceFormatsKHR(surface));
        m_SwapFormat = chooseSwapSurfaceFormat(availableFormats);

        const vk::SurfaceFormatKHR surfaceFormat =
            chooseSwapSurfaceFormat(vkCheck(physicalDevice.getSurfaceFormatsKHR(surface)));

        std::vector<vk::PresentModeKHR> availablePresentModes = vkCheck(physicalDevice.getSurfacePresentModesKHR(surface));

        vk::SwapchainCreateInfoKHR swapchainCI{
            .surface = surface,
            .minImageCount = minImageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = m_SwapExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCaps.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = chooseSwapPresentMode(desiredPresentMode, availablePresentModes),
            .clipped = vk::True,
            .oldSwapchain = nullptr,
        };

        m_SwapChain = vkCheck(device.createSwapchainKHRUnique(swapchainCI));
        m_SwapChainImages = vkCheck(device.getSwapchainImagesKHR(*m_SwapChain));
        for (size_t i = 0; i < m_SwapChainImages.size(); ++i)
        {
            setDebugName(device, m_SwapChainImages[i], "SwapChainImage" + std::to_string(i));
        }

        vk::ImageViewCreateInfo viewCI{
            .viewType = vk::ImageViewType::e2D,
            .format = surfaceFormat.format,
            .subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        };

        m_SwapChainImageViews.reserve(m_SwapChainImages.size());
        for (uint32_t i = 0; i < m_SwapChainImages.size(); ++i)
        {
            viewCI.image = m_SwapChainImages[i];
            m_SwapChainImageViews.push_back(std::move(vkCheck(device.createImageViewUnique(viewCI))));
            setDebugName(device, *m_SwapChainImageViews[i], "SwapChainImageView" + std::to_string(i));
        }
    }
}
