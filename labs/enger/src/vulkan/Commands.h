#pragma once

#include "vk.h"

#include "Resources.h"

namespace enger
{
    class Device;
    struct SubmitHandle;
    class Queue;

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

    static constexpr auto kMaxTransitionImages = 12;
    class TransitionImageBuilder final
    {
    public:
        TransitionImageBuilder(Device& device) : device_(device) {}
        void add(TextureHandle texHandle, vk::ImageLayout srcLayout, vk::ImageLayout dstLayout);
        vk::DependencyInfo build();
        void clear() { imageCount_ = 0; }
    private:
        Device& device_;
        std::array<vk::ImageMemoryBarrier2, kMaxTransitionImages> imageBarriers_;
        uint32_t imageCount_ = 0;
    };
    struct TransitionImageInfo final
    {
        TextureHandle texHandle;
        vk::ImageLayout srcLayout{};
        vk::ImageLayout dstLayout{};
    };

    // TODO improve the overall API & impl. Remove friend classes
    class CommandBuffer final
    {
        friend class Device;
    public:
        CommandBuffer() = default;
        CommandBuffer(CommandBuffer&) = delete;
        CommandBuffer& operator=(CommandBuffer&) = delete;
        CommandBuffer(CommandBuffer&&) = default;
        CommandBuffer& operator=(CommandBuffer&&) = default;
        ~CommandBuffer() = default;

        // temporary
        vk::CommandBuffer& get() { return m_CommandBuffer; }

        void begin(vk::CommandBufferUsageFlags flags);
        void end();
        void reset();
        void transitionImage(TextureHandle texHandle, vk::ImageLayout srcLayout, vk::ImageLayout dstLayout);

        void transitionImages(std::span<const TransitionImageInfo> infos);
        void transitionImages(vk::DependencyInfo info);

        struct TransferTextureDesc
        {
            TextureHandle handle;
            vk::ImageLayout srcLayout = vk::ImageLayout::eGeneral;
            vk::ImageLayout dstLayout = vk::ImageLayout::eGeneral;
            std::optional<vk::AccessFlags2> srcAccess;
            std::optional<vk::AccessFlags2> dstAccess;
            std::optional<vk::PipelineStageFlags2> srcStage;
            std::optional<vk::PipelineStageFlags2> dstStage;
            Queue* srcQueue = nullptr;
            Queue* dstQueue = nullptr;
        };
        void imageBarrier(TransferTextureDesc desc);

        struct TransferBufferDesc
        {
            std::span<const BufferHandle> handles;
            vk::AccessFlags2 srcAccess = vk::AccessFlagBits2::eTransferRead;
            vk::AccessFlags2 dstAccess = vk::AccessFlagBits2::eTransferWrite;
            vk::PipelineStageFlags2 srcStage = vk::PipelineStageFlagBits2::eTransfer;
            vk::PipelineStageFlags2 dstStage = vk::PipelineStageFlagBits2::eTransfer;
            Queue& srcQueue;
            Queue& dstQueue;
        };
        /// This returns the SubmitHandle for the RELEASE operation.
        /// Release must complete BEFORE this command buffer (ACQUIRE) is submitted on the dst queue.
        /// Therefore, you should add a waitTimeline for this SubmitHandle.
        [[nodiscard]] SubmitHandle bufferBarrier(TransferBufferDesc desc);

        void blitImage(TextureHandle srcTexHandle, TextureHandle dstTexHandle);
        void clearColorImage(TextureHandle texHandle, vk::ClearColorValue color, vk::ImageAspectFlags aspectMask);

        void copyBuffer(BufferHandle srcBuffer, BufferHandle dstBuffer, vk::BufferCopy region);
        void copyBufferToImage(BufferHandle buffer, TextureHandle image, std::span<const vk::BufferImageCopy> regions);
        void copyBufferToImage(BufferHandle buffer, TextureHandle image, vk::BufferImageCopy& region);
        void copyBufferToImage2(vk::CopyBufferToImageInfo2& info);

        void setViewport(vk::Viewport& viewport);
        void setScissor(vk::Rect2D& scissor);

        void beginRendering(vk::RenderingInfo& renderingInfo);
        void endRendering();

        void bindComputePipeline(ComputePipelineHandle pipelineHandle);
        void bindGraphicsPipeline(GraphicsPipelineHandle pipelineHandle);
        void bindDescriptorSets(vk::PipelineBindPoint bindPoint, PipelineLayoutHandle pipelineLayout, uint32_t firstSet, std::span<const vk::
                                DescriptorSet> descriptorSets);
        // Helper function to automatically bind the descriptor sets for bindless.
        void bindDescriptorSetsBindless(vk::PipelineBindPoint bindPoint);
        void pushConstants(PipelineLayoutHandle pipelineLayout, vk::ShaderStageFlags stages, uint32_t offset, uint32_t size, const void* data);
        void bindIndexBuffer(BufferHandle buffer, uint32_t offset, vk::IndexType indexType);

        void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
        void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

    private:
        CommandBuffer(Device* device, vk::CommandBuffer commandBuffer);
        Device* m_Device = nullptr;
        vk::CommandBuffer m_CommandBuffer;
    };
}
