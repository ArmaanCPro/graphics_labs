#pragma once


#include "Imgui.h"
#include "vulkan/Device.h"
#include "vulkan/SwapChain.h"

#include <glm/glm.hpp>

#include "Framing.h"
#include "MeshLoader.h"
#include "Scene/SceneGraph.h"

#include "Stats.h"

namespace enger
{
    struct GridPushConstants
    {
        alignas(4) glm::mat4 mvp;
        alignas(4) glm::vec4 camPos;
        alignas(4) glm::vec4 origin;
    };

    struct TonemapperPushConstants
    {
        alignas(4) uint32_t srcTexIndex;
        alignas(4) uint32_t samplerIndex;
    };

    class ENGER_EXPORT Renderer
    {
    public:
        Renderer(Device& device, SwapChain& swapchain);

        void render(framing::FrameContext& frameContext, const DrawContext& dctx, EngineStats& stats, bool drawGrid = true);

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

        Holder<PipelineLayoutHandle> m_GridPipelineLayout;
        Holder<GraphicsPipelineHandle> m_GridPipeline;

        Holder<PipelineLayoutHandle> m_TonemapperPipelineLayout;
        Holder<GraphicsPipelineHandle> m_TonemapperPipeline;

        static constexpr vk::SampleCountFlagBits m_MsaaSamples = vk::SampleCountFlagBits::e4;

        bool m_ShouldResize = false;
        uint32_t m_PendingWidth = 0;
        uint32_t m_PendingHeight = 0;

        bool m_IsFirstFrame = true;
    };
}
