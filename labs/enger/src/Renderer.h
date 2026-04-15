#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <fastgltf/types.hpp>

#include "Imgui.h"
#include "vulkan/Descriptors.h"
#include "vulkan/Device.h"
#include "vulkan/SwapChain.h"

#include <glm/glm.hpp>

#include "Framing.h"
#include "MeshLoader.h"
#include "SceneGraph.h"

#include "Stats.h"

namespace enger
{
    class Renderer
    {
    public:
        Renderer(Device& device, SwapChain& swapchain);

        void render(framing::FrameContext& frameContext, const DrawContext& dctx, EngineStats& stats);

        void onResize(uint32_t width, uint32_t height);

        vk::Format renderFormat() const { return m_Device.getImage(m_RenderTarget)->format_; }
        vk::Format depthFormat() const { return m_Device.getImage(m_DepthBuffer)->format_; }
        vk::SampleCountFlagBits msaaSamples() const { return m_MsaaSamples; }

    private:
        void createRenderTextures(uint32_t width, uint32_t height);

        Device& m_Device;
        SwapChain& m_SwapChain;
        Queue& m_GraphicsQueue;

        uint32_t m_CurrentFrame = 0;

        // Rendering happens on this texture.
        Holder<TextureHandle> m_MsaaRenderTarget;
        // This is an intermediate target that gets resolved to from the Msaa target and blitted to the swapchain. Required because image format and swapchain format are different.
        Holder<TextureHandle> m_RenderTarget;
        Holder<TextureHandle> m_DepthBuffer;

        static constexpr vk::SampleCountFlagBits m_MsaaSamples = vk::SampleCountFlagBits::e4;

        bool m_ShouldResize = false;
        uint32_t m_PendingWidth = 0;
        uint32_t m_PendingHeight = 0;
    };
}
