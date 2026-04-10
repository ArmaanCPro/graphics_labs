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

    CommandBuffer::CommandBuffer(Device *device, vk::CommandBuffer commandBuffer)
        :
        m_Device(device), m_CommandBuffer(commandBuffer)
    {}
}
