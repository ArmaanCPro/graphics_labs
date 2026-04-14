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

    // This is an abstraction for a UI Layer. Not necessarily needed as we only use ImGui (for now)
    class UILayer
    {
    public:
        virtual ~UILayer() = default;

        virtual void beginFrame() = 0;
        // This could be a user-defined draw function later, but for now it is built-in.
        virtual void draw() = 0;
        virtual void endFrame(framing::FrameContext& fctx) = 0;

        virtual void postRenderFinished() = 0;

        virtual void onResize(uint32_t width, uint32_t height) = 0;
    };

    class ImguiLayer : public UILayer
    {
    public:
        ImguiLayer(Instance& instance, Device& device, GlfwWindow& window, SwapChain& swapchain);
        ~ImguiLayer() override;

        void beginFrame() override;
        void draw() override;
        void endFrame(framing::FrameContext& fctx) override;

        // not sure if this is needed or can be merged into endFrame(...)
        void postRenderFinished() override;

        void onResize(uint32_t width, uint32_t height) override;

    private:
        Device& m_Device;
        SwapChain& m_Swapchain;
        GlfwWindow& m_Window;
        DescriptorAllocator m_DescriptorAllocator;
    };
}
