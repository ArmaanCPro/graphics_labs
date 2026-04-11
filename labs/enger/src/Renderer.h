#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include "vulkan/Descriptors.h"
#include "vulkan/Device.h"
#include "vulkan/SwapChain.h"

namespace enger
{
    constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    class Renderer
    {
    public:
        Renderer(Device& device, SwapChain& swapchain);
        ~Renderer();

        void drawFrame();

    private:
        Device& m_Device;
        SwapChain& m_SwapChain;

        uint32_t m_CurrentFrame = 0;
        uint64_t m_FrameNumber = 0;

        std::array<UniqueCommandPool, FRAMES_IN_FLIGHT> m_CommandPools;
        // the cmdbuf will be destroyed when its parent pool is destroyed, so it doesn't need to be unique
        std::array<CommandBuffer, FRAMES_IN_FLIGHT> m_CommandBuffers;

        std::array<vk::UniqueSemaphore, FRAMES_IN_FLIGHT> m_ImageAvailableSemaphores;
        // this has as many elements as there are swapchain images
        std::vector<vk::UniqueSemaphore> m_RenderFinishedSemaphores;

        DescriptorAllocator m_DescriptorAllocator;
        vk::DescriptorSet m_RenderTargetDescriptor;

        Holder<DescriptorSetLayoutHandle> m_RenderTargetDescriptorLayout;
        Holder<TextureHandle> m_RenderTarget;

        Holder<PipelineLayoutHandle> m_GradientPipelineLayout;
        Holder<ComputePipelineHandle> m_GradientPipeline;
    };
}
