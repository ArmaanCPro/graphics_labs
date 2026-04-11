#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "vulkan/Descriptors.h"

namespace enger
{
    class Instance;
    class Device;
    class SwapChain;

    class ImguiLayer
    {
    public:
        ImguiLayer(Instance& instance, Device& device, GLFWwindow* window, SwapChain& swapchain);
        ~ImguiLayer();

        void draw(vk::CommandBuffer cmd, vk::ImageView targetImageView);

    private:
        Device& m_Device;
        SwapChain& m_Swapchain;
        DescriptorAllocator m_DescriptorAllocator;
    };
}
