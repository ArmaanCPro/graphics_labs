#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "Framing.h"
#include "GlfwWindow.h"
#include "vulkan/Descriptors.h"

namespace enger
{
    class Instance;
    class Device;
    class SwapChain;

    class ImguiLayer : public framing::IFrameLayer
    {
    public:
        ImguiLayer(Instance& instance, Device& device, GlfwWindow& window, SwapChain& swapchain);
        ~ImguiLayer() override;

        void draw(framing::FrameContext& ctx) override;

    private:
        Device& m_Device;
        DescriptorAllocator m_DescriptorAllocator;
    };
}
