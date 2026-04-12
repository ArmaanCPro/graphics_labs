#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include "Imgui.h"
#include "vulkan/Descriptors.h"
#include "vulkan/Device.h"
#include "vulkan/SwapChain.h"

#include <glm/glm.hpp>

#include "Framing.h"
#include "MeshLoader.h"

namespace enger
{
    constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    struct ComputePushConstants
    {
        alignas(16) glm::vec4 data1;
        alignas(16) glm::vec4 data2;
        alignas(4) uint32_t textureIndex;
    };

    struct DrawPushConstants
    {
        glm::mat4 worldMatrix;
        vk::DeviceAddress vertexBufferDeviceAddress;
    };

    class Renderer : public framing::IFrameLayer
    {
    public:
        Renderer(Device& device, SwapChain& swapchain);

        void draw(framing::FrameContext& frameContext) override;

        void onResize(uint32_t width, uint32_t height) override;

    private:

        void createRenderTextures(uint32_t width, uint32_t height);

        Device& m_Device;
        SwapChain& m_SwapChain;
        Queue& m_GraphicsQueue;

        uint32_t m_CurrentFrame = 0;

        Holder<TextureHandle> m_RenderTarget;
        Holder<TextureHandle> m_DepthBuffer;

        Holder<PipelineLayoutHandle> m_GradientPipelineLayout;
        Holder<ComputePipelineHandle> m_GradientPipeline;

        Holder<PipelineLayoutHandle> m_GraphicsPipelineLayout;
        Holder<GraphicsPipelineHandle> m_GraphicsPipeline;

        std::vector<std::shared_ptr<MeshAsset>> m_TestMeshes;
    };
}
