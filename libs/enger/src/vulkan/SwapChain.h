#pragma once

#include "vk.h"

#include <GLFW/glfw3.h>

#include "Device.h"
#include "GlfwWindow.h"

namespace enger
{
    class SwapChain
    {
    public:
        // TODO cleanly refactor to use Device texture pools nicely
        SwapChain(Device& device, vk::SurfaceKHR surface, const GlfwWindow& window,
                  vk::PresentModeKHR desiredPresentMode);

        ~SwapChain();

        void recreate(uint32_t width, uint32_t height, std::optional<vk::PresentModeKHR> desiredPresentMode = {});

        vk::Result present(std::span<const vk::Semaphore> waitSemaphores, uint32_t imageIndex, Queue& queue);

        [[nodiscard]] vk::SwapchainKHR swapChain() { return *m_SwapChain; }

        [[nodiscard]] TextureHandle swapChainImageHandle(uint32_t index) { return m_SwapchainImageHandles[index]; }

        [[nodiscard]] vk::Image swapChainImage(uint32_t index) { return m_SwapChainImages[index]; }
        [[nodiscard]] vk::ImageView swapChainImageView(uint32_t index) { return *m_SwapChainImageViews[index]; }
        [[nodiscard]] vk::ImageView swapChainUnormImageView(uint32_t index) { return *m_UnormImageViews[index]; }
        [[nodiscard]] uint32_t numSwapChainImages() const { return static_cast<uint32_t>(m_SwapChainImages.size()); }
        [[nodiscard]] vk::Extent2D swapChainExtent() const { return m_SwapExtent; }
        [[nodiscard]] vk::Format swapChainFormat() const { return m_SwapFormat.format; }
        [[nodiscard]] vk::Format swapChainUnormFormat() const { return getUnormEquivalent(m_SwapFormat.format); }

    private:
        void createSwapChain(uint32_t width, uint32_t height, vk::PresentModeKHR desiredPresentMode,
                             std::optional<vk::SwapchainKHR> oldSwapchain = {});

        void destroySwapchainHandles();

        static vk::Format getUnormEquivalent(vk::Format srgbFormat);

    private:
        Device& m_Device;
        vk::Extent2D m_SwapExtent;
        vk::SurfaceKHR m_Surface; // non-owning
        vk::SurfaceFormatKHR m_SwapFormat;
        vk::UniqueSwapchainKHR m_SwapChain;

        vk::PresentModeKHR m_CurrentPresentMode;

        // TEMP, need to refactor better, a texture handle representing swapchain images
        std::vector<TextureHandle> m_SwapchainImageHandles;

        /// These are managed by the swapchain, so no need to make them a unique handle.
        /// Consider changing this to a TextureHandle
        std::vector<vk::Image> m_SwapChainImages;

        /// These unique handles store the device for deletion's sake. The SwapChain is long-lived, so it doesn't matter.
        /// For constrained environments, be mindful of the memory footprint.
        std::vector<vk::UniqueImageView> m_SwapChainImageViews;
        // Needed for ImGui, and anything else that uses a linear color space.
        std::vector<vk::UniqueImageView> m_UnormImageViews;
    };
}
