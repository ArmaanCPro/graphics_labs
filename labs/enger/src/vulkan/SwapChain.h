#pragma once

#include "vk.h"

#include <GLFW/glfw3.h>

namespace enger
{
    class SwapChain
    {
    public:
        SwapChain(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface, GLFWwindow* window, vk::PresentModeKHR desiredPresentMode);

        [[nodiscard]] vk::SwapchainKHR swapChain() { return *m_SwapChain; }

        [[nodiscard]] vk::Image swapChainImage(uint32_t index) { return m_SwapChainImages[index]; }
        [[nodiscard]] uint32_t numSwapChainImages() { return static_cast<uint32_t>(m_SwapChainImages.size()); }

    private:
        vk::Extent2D m_SwapExtent;
        vk::SurfaceFormatKHR m_SwapFormat;
        vk::UniqueSwapchainKHR m_SwapChain;

        /// These are managed by the swapchain, so no need to make them a unique handle.
        /// Consider changing this to a TextureHandle
        std::vector<vk::Image> m_SwapChainImages;

        /// These unique handles store the device for deletion's sake. The SwapChain is long-lived, so it doesn't matter.
        /// For constrained environments, be mindful of the memory footprint.
        std::vector<vk::UniqueImageView> m_SwapChainImageViews;
    };
}
