#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include "Imgui.h"
#include "vulkan/Descriptors.h"
#include "vulkan/Device.h"
#include "vulkan/SwapChain.h"

#include <glm/glm.hpp>

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

    class Renderer
    {
    public:
        Renderer(Instance& instance, Device& device, SwapChain& swapchain, GLFWwindow* window);
        ~Renderer();

        void drawFrame();

    private:

        Device& m_Device;
        SwapChain& m_SwapChain;
        Queue& m_GraphicsQueue;

        ImguiLayer m_ImguiLayer;

        uint32_t m_CurrentFrame = 0;

        std::array<SubmitHandle, FRAMES_IN_FLIGHT> m_LastFrameSubmits = {0};

        std::array<UniqueCommandPool, FRAMES_IN_FLIGHT> m_CommandPools;
        // the cmdbuf will be destroyed when its parent pool is destroyed, so it doesn't need to be unique
        std::array<CommandBuffer, FRAMES_IN_FLIGHT> m_CommandBuffers;

        std::array<vk::UniqueSemaphore, FRAMES_IN_FLIGHT> m_ImageAvailableSemaphores;
        // this has as many elements as there are swapchain images
        std::vector<vk::UniqueSemaphore> m_RenderFinishedSemaphores;

        Holder<TextureHandle> m_RenderTarget;
        Holder<TextureHandle> m_DepthBuffer;

        Holder<PipelineLayoutHandle> m_GradientPipelineLayout;
        Holder<ComputePipelineHandle> m_GradientPipeline;

        Holder<PipelineLayoutHandle> m_GraphicsPipelineLayout;
        Holder<GraphicsPipelineHandle> m_GraphicsPipeline;

        std::vector<std::shared_ptr<MeshAsset>> m_TestMeshes;
    };
}
