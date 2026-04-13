#include "Commands.h"

#include "Device.h"

#include "GpuResourceTypes.h"

namespace enger
{
    void CommandBuffer::begin(vk::CommandBufferUsageFlags flags)
    {
        vkCheck(m_CommandBuffer.begin(vk::CommandBufferBeginInfo{
            .flags = flags,
        }));
    }

    void CommandBuffer::end()
    {
        vkCheck(m_CommandBuffer.end());
    }

    void CommandBuffer::reset()
    {
        vkCheck(m_CommandBuffer.reset());
    }

    void CommandBuffer::transitionImage(TextureHandle texHandle, vk::ImageLayout srcLayout, vk::ImageLayout dstLayout)
    {
        assert(m_Device != nullptr);
        auto* image = m_Device->getImage(texHandle);
        assert(image != nullptr);

        vk::ImageMemoryBarrier2 barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            .oldLayout = srcLayout,
            .newLayout = dstLayout,
            .image = image->image_,
            .subresourceRange = vk::ImageSubresourceRange{
                .aspectMask = dstLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = vk::RemainingMipLevels,
                .baseArrayLayer = 0,
                .layerCount = vk::RemainingArrayLayers,
            }
        };

        vk::DependencyInfo dependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier,
        };

        m_CommandBuffer.pipelineBarrier2(dependencyInfo);
    }

    void CommandBuffer::blitImage(TextureHandle srcTexHandle, TextureHandle dstTexHandle)
    {
        assert(m_Device != nullptr);
        auto* srcImage = m_Device->getImage(srcTexHandle);
        assert(srcImage != nullptr);
        auto* dstImage = m_Device->getImage(dstTexHandle);
        assert(dstImage != nullptr);

        vk::Extent2D srcExtent = { srcImage->extent_.width, srcImage->extent_.height };
        vk::Extent2D dstExtent = { dstImage->extent_.width, dstImage->extent_.height };

        vk::ImageBlit2 blitRegion{};
        blitRegion.srcOffsets[1].x = srcExtent.width;
        blitRegion.srcOffsets[1].y = srcExtent.height;
        blitRegion.srcOffsets[1].z = 1;
        blitRegion.dstOffsets[1].x = dstExtent.width;
        blitRegion.dstOffsets[1].y = dstExtent.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.srcSubresource.mipLevel = 0;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.dstSubresource.mipLevel = 0;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;

        vk::BlitImageInfo2 blitInfo{
            .srcImage = srcImage->image_,
            .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
            .dstImage = dstImage->image_,
            .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
            .regionCount = 1,
            .pRegions = &blitRegion,
            .filter = vk::Filter::eLinear,
        };

        m_CommandBuffer.blitImage2(blitInfo);
    }

    void CommandBuffer::clearColorImage(TextureHandle texHandle, vk::ClearColorValue color, vk::ImageAspectFlags aspectMask)
    {
        assert(m_Device != nullptr);
        auto* image = m_Device->getImage(texHandle);
        assert(image != nullptr);

        vk::ImageSubresourceRange clearRange{
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = vk::RemainingMipLevels,
            .baseArrayLayer = 0,
            .layerCount = vk::RemainingArrayLayers,
        };

        m_CommandBuffer.clearColorImage(image->image_, vk::ImageLayout::eGeneral, &color, 1, &clearRange);
    }

    void CommandBuffer::copyBuffer(BufferHandle srcBuffer, BufferHandle dstBuffer, vk::BufferCopy region)
    {
        assert(m_Device != nullptr);
        auto* srcBufferObj = m_Device->getBuffer(srcBuffer);
        assert(srcBufferObj != nullptr);
        auto* dstBufferObj = m_Device->getBuffer(dstBuffer);
        assert(dstBufferObj != nullptr);

        m_CommandBuffer.copyBuffer(srcBufferObj->buffer_, dstBufferObj->buffer_, 1, &region);
    }

    void CommandBuffer::copyBufferToImage(BufferHandle buffer, TextureHandle image, vk::BufferImageCopy region)
    {
        assert(m_Device != nullptr);
        auto* bufferObj = m_Device->getBuffer(buffer);
        assert(bufferObj != nullptr);
        auto* imageObj = m_Device->getImage(image);
        assert(imageObj != nullptr);

        m_CommandBuffer.copyBufferToImage(bufferObj->buffer_, imageObj->image_, vk::ImageLayout::eTransferDstOptimal, 1, &region);
    }

    void CommandBuffer::setViewport(vk::Viewport& viewport)
    {
        m_CommandBuffer.setViewport(0, 1, &viewport);
    }

    void CommandBuffer::setScissor(vk::Rect2D& scissor)
    {
        m_CommandBuffer.setScissor(0, 1, &scissor);
    }

    void CommandBuffer::beginRendering(vk::RenderingInfo &renderingInfo)
    {
        m_CommandBuffer.beginRendering(renderingInfo);
    }

    void CommandBuffer::endRendering()
    {
        m_CommandBuffer.endRendering();
    }

    void CommandBuffer::bindComputePipeline(ComputePipelineHandle pipelineHandle)
    {
        assert(m_Device != nullptr);
        auto* pipeline = m_Device->getComputePipeline(pipelineHandle);
        assert(pipeline != nullptr);

        m_CommandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->handle);
    }

    void CommandBuffer::bindGraphicsPipeline(GraphicsPipelineHandle pipelineHandle)
    {
        assert(m_Device != nullptr);
        auto* pipeline = m_Device->getGraphicsPipeline(pipelineHandle);
        assert(pipeline != nullptr);

        m_CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->handle);
    }

    void CommandBuffer::bindDescriptorSets(vk::PipelineBindPoint bindPoint, PipelineLayoutHandle pipelineLayout,
                                           uint32_t firstSet, std::span<const vk::DescriptorSet> descriptorSets)
    {
        assert(m_Device != nullptr);
        auto* layout = m_Device->getPipelineLayout(pipelineLayout);
        assert(layout != nullptr);

        m_CommandBuffer.bindDescriptorSets(bindPoint, layout->layout, firstSet, descriptorSets, nullptr);
    }

    void CommandBuffer::pushConstants(PipelineLayoutHandle pipelineLayout, vk::ShaderStageFlags stages,
                                      uint32_t offset, uint32_t size, const void *data)
    {
        assert(m_Device != nullptr);
        auto* layout = m_Device->getPipelineLayout(pipelineLayout);
        assert(layout != nullptr);

        m_CommandBuffer.pushConstants(layout->layout, stages, offset, size, data);
    }

    void CommandBuffer::bindIndexBuffer(BufferHandle buffer, uint32_t offset, vk::IndexType indexType)
    {
        assert(m_Device != nullptr);
        auto* rawBuf = m_Device->getBuffer(buffer);
        assert(rawBuf != nullptr);

        m_CommandBuffer.bindIndexBuffer(rawBuf->buffer_, offset, indexType);
    }

    void CommandBuffer::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        m_CommandBuffer.dispatch(groupCountX, groupCountY, groupCountZ);
    }

    void CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        m_CommandBuffer.draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
        int32_t vertexOffset, uint32_t firstInstance)
    {
        m_CommandBuffer.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    CommandBuffer::CommandBuffer(Device *device, vk::CommandBuffer commandBuffer)
        :
        m_Device(device), m_CommandBuffer(commandBuffer)
    {}
}
