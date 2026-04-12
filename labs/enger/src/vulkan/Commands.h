#pragma once

#include "vk.h"

#include "Resources.h"

namespace enger
{
    class Device;

    enum class CommandPoolFlags
    {
        Transient,
        ResetCommandBuffer,
    };

    struct UniqueCommandPool
    {
        vk::UniqueCommandPool m_CommandPool;
        uint32_t m_QueueFamilyIndex;
    };

    enum class CommandBufferLevel
    {
        Primary,
        //Secondary,
    };

    // TODO improve the overall API & impl. Remove friend classes
    class CommandBuffer
    {
        friend class Device;
    public:
        CommandBuffer() = default;
        // temporary
        vk::CommandBuffer& get() { return m_CommandBuffer; }

        void begin(vk::CommandBufferUsageFlags flags);
        void end();
        void reset();
        void transitionImage(TextureHandle texHandle, vk::ImageLayout srcLayout, vk::ImageLayout dstLayout);
        void blitImage(TextureHandle srcTexHandle, TextureHandle dstTexHandle);
        void clearColorImage(TextureHandle texHandle, vk::ClearColorValue color, vk::ImageAspectFlags aspectMask);

        void setViewport(vk::Viewport& viewport);
        void setScissor(vk::Rect2D& scissor);

        void beginRendering(vk::RenderingInfo& renderingInfo);
        void endRendering();

        void bindComputePipeline(ComputePipelineHandle pipelineHandle);
        void bindGraphicsPipeline(GraphicsPipelineHandle pipelineHandle);
        void bindDescriptorSets(vk::PipelineBindPoint bindPoint, PipelineLayoutHandle pipelineLayout, uint32_t firstSet, std::span<vk::DescriptorSet> descriptorSets);
        void pushConstants(PipelineLayoutHandle pipelineLayout, vk::ShaderStageFlags stages, uint32_t offset, uint32_t size, const void* data);

        void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
        void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);

    private:
        CommandBuffer(Device* device, vk::CommandBuffer commandBuffer);
        Device* m_Device;
        vk::CommandBuffer m_CommandBuffer;
    };
}
