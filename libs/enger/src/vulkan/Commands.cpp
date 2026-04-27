#include "Commands.h"

#include "Device.h"

#include "GpuResourceTypes.h"
#include "QueueSubmitBuilder.h"
#include "Logging/Assert.h"

#include "Profiling/Profiler.h"

namespace
{
    std::pair<vk::AccessFlags2, vk::PipelineStageFlags2> getTransitionAccessAndStage(vk::ImageLayout layout)
    {
        vk::AccessFlags2 access = vk::AccessFlagBits2::eMemoryWrite;
        vk::PipelineStageFlags2 stage = vk::PipelineStageFlagBits2::eAllCommands;
        if (layout == vk::ImageLayout::eTransferSrcOptimal)
        {
            access = vk::AccessFlagBits2::eTransferRead;
            stage = vk::PipelineStageFlagBits2::eTransfer;
        }
        else if (layout == vk::ImageLayout::eTransferDstOptimal)
        {
            access = vk::AccessFlagBits2::eTransferWrite;
            stage = vk::PipelineStageFlagBits2::eTransfer;
        }
        else if (layout == vk::ImageLayout::eColorAttachmentOptimal)
        {
            access = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite;
            stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        }
        else if (layout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
        {
            access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
            stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests;
        }
        else if (layout == vk::ImageLayout::ePresentSrcKHR)
        {
            access = vk::AccessFlagBits2::eNone;
            stage = vk::PipelineStageFlagBits2::eBottomOfPipe; // swapchain happens at the end of the pipeline. just make sure the present waits for AllGraphics/AllCommands too
        }
        return {access, stage};
    }
}

namespace enger
{
    void TransitionImageBuilder::add(TextureHandle texHandle, vk::ImageLayout srcLayout, vk::ImageLayout dstLayout)
    {
        auto* image = device_.getImage(texHandle);
        EASSERT(image);

        auto [srcAccess, srcStage] = getTransitionAccessAndStage(srcLayout);
        auto [dstAccess, dstStage] = getTransitionAccessAndStage(dstLayout);

        imageBarriers_.push_back(vk::ImageMemoryBarrier2{
            .srcAccessMask = srcAccess,
            .dstAccessMask = dstAccess,
            .oldLayout = srcLayout,
            .newLayout = dstLayout,
            .image = image->image_,
            .subresourceRange = vk::ImageSubresourceRange{
                .aspectMask = dstLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal
                                  ? vk::ImageAspectFlagBits::eDepth
                                  : vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = vk::RemainingMipLevels,
            }
        });
    }

    vk::DependencyInfo TransitionImageBuilder::build()
    {
        vk::DependencyInfo dependencyInfo{
            .imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers_.size()),
            .pImageMemoryBarriers = imageBarriers_.data(),
        };
        return dependencyInfo;
    }

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
        EASSERT(m_Device != nullptr);
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_BARRIER)

        auto* image = m_Device->getImage(texHandle);
        EASSERT(image != nullptr);

        auto [srcAccess, srcStage] = getTransitionAccessAndStage(srcLayout);
        auto [dstAccess, dstStage] = getTransitionAccessAndStage(dstLayout);

        image->state_ = ImageState{
            .layout = dstLayout,
            .access = dstAccess,
            .stage = dstStage,
        };

        vk::ImageMemoryBarrier2 barrier{
            .srcStageMask = srcStage,
            .srcAccessMask = srcAccess,
            .dstStageMask = dstStage,
            .dstAccessMask = dstAccess,
            .oldLayout = srcLayout,
            .newLayout = dstLayout,
            .image = image->image_,
            .subresourceRange = vk::ImageSubresourceRange{
                .aspectMask = dstLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal
                                  ? vk::ImageAspectFlagBits::eDepth
                                  : vk::ImageAspectFlagBits::eColor,
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

    void CommandBuffer::transitionImages(std::span<const TransitionImageInfo> infos)
    {
        EASSERT(m_Device != nullptr);
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_BARRIER)

        EASSERT(infos.size() <= kMaxTransitionImages);

        std::array<vk::ImageMemoryBarrier2, kMaxTransitionImages> barriers;
        for (uint32_t i = 0; i < infos.size(); ++i)
        {
            const auto info = infos[i];
            auto* image = m_Device->getImage(info.texHandle);
            EASSERT(image != nullptr);

            auto [srcAccess, srcStage] = getTransitionAccessAndStage(info.srcLayout);
            auto [dstAccess, dstStage] = getTransitionAccessAndStage(info.dstLayout);

            image->state_ = ImageState{
                .layout = info.dstLayout,
                .access = dstAccess,
                .stage = dstStage,
            };

            barriers[i] = vk::ImageMemoryBarrier2{
                .srcStageMask = srcStage,
                .srcAccessMask = srcAccess,
                .dstStageMask = dstStage,
                .dstAccessMask = dstAccess,
                .oldLayout = info.srcLayout,
                .newLayout = info.dstLayout,
                .image = image->image_,
                .subresourceRange = vk::ImageSubresourceRange{
                    .aspectMask = info.dstLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal
                                      ? vk::ImageAspectFlagBits::eDepth
                                      : vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = vk::RemainingMipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = vk::RemainingArrayLayers,
                }
            };
        }

        vk::DependencyInfo dependencyInfo{
            .imageMemoryBarrierCount = static_cast<uint32_t>(infos.size()),
            .pImageMemoryBarriers = barriers.data(),
        };
        m_CommandBuffer.pipelineBarrier2(dependencyInfo);
    }

    void CommandBuffer::transitionImages(vk::DependencyInfo info)
    {
        m_CommandBuffer.pipelineBarrier2(info);
    }

    void CommandBuffer::imageBarrier(TransferTextureDesc desc)
    {
        EASSERT(m_Device != nullptr);
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_BARRIER)

        auto* image = m_Device->getImage(desc.handle);
        EASSERT(image != nullptr);

        if (!desc.srcAccess.has_value())
            desc.srcAccess = getTransitionAccessAndStage(desc.srcLayout).first;
        if (!desc.dstAccess.has_value())
            desc.dstAccess = getTransitionAccessAndStage(desc.dstLayout).first;
        if (!desc.srcStage.has_value())
            desc.srcStage = getTransitionAccessAndStage(desc.srcLayout).second;
        if (!desc.dstStage.has_value())
            desc.dstStage = getTransitionAccessAndStage(desc.dstLayout).second;

        if (desc.srcQueue || desc.dstQueue)
        {
            EASSERT(desc.srcQueue && desc.dstQueue);

            vk::ImageMemoryBarrier2 releaseBarrier{
                .srcStageMask = desc.srcStage.value(),
                .srcAccessMask = desc.srcAccess.value(),
                .dstStageMask = vk::PipelineStageFlagBits2::eNone, // ignored
                .dstAccessMask = vk::AccessFlagBits2::eNone, // ignored
                .oldLayout = desc.srcLayout,
                .newLayout = desc.dstLayout,
                .srcQueueFamilyIndex = desc.srcQueue->familyIndex(),
                .dstQueueFamilyIndex = desc.dstQueue->familyIndex(),
                .image = image->image_,
                .subresourceRange = vk::ImageSubresourceRange{
                    .aspectMask = desc.dstLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal
                                      ? vk::ImageAspectFlagBits::eDepth
                                      : vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = vk::RemainingMipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = vk::RemainingArrayLayers,
                },
            };
            vk::DependencyInfo releaseInfo{
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &releaseBarrier,
            };

            SubmitHandle releaseSubmission = desc.srcQueue->submitImmediateAsync([&](CommandBuffer& cmd) {
                cmd.get().pipelineBarrier2(releaseInfo);
            });

            vk::ImageMemoryBarrier2 acquireBarrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eNone, // ignored
                .srcAccessMask = vk::AccessFlagBits2::eNone, // ignored
                .dstStageMask = desc.dstStage.value(),
                .dstAccessMask = desc.dstAccess.value(),
                .oldLayout = desc.srcLayout,
                .newLayout = desc.dstLayout,
                .srcQueueFamilyIndex = desc.dstQueue->familyIndex(),
                .dstQueueFamilyIndex = desc.srcQueue->familyIndex(),
                .image = image->image_,
                .subresourceRange = vk::ImageSubresourceRange{
                    .aspectMask = desc.dstLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal
                                      ? vk::ImageAspectFlagBits::eDepth
                                      : vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = vk::RemainingMipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = vk::RemainingArrayLayers,
                },
            };
            vk::DependencyInfo acquireInfo{
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &acquireBarrier,
            };

            QueueSubmitBuilder acquireBuilder;
            acquireBuilder.waitTimeline(desc.srcQueue->timelineSemaphore(), releaseSubmission, vk::PipelineStageFlagBits2::eTransfer);
            desc.dstQueue->submitImmediateAsync([&](CommandBuffer& cmd) {
                cmd.get().pipelineBarrier2(acquireInfo);
            }, acquireBuilder.build());
        }
    }

    SubmitHandle CommandBuffer::bufferBarrier(TransferBufferDesc desc)
    {
        EASSERT(m_Device != nullptr);
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_BARRIER)

        if (m_Device->physicalDeviceInfo().hasMaintenance9)
        {
            // With Maintenance9, we don't need a barrier for a release submission, just a semaphore for sync
            auto releaseSubmission = desc.srcQueue.submitImmediateAsync([](CommandBuffer&) {});

            std::vector<vk::BufferMemoryBarrier2> acquireBarriers;
            acquireBarriers.reserve(desc.handles.size());

            for (auto& handle : desc.handles)
            {
                acquireBarriers.push_back(vk::BufferMemoryBarrier2{
                    .srcStageMask = desc.srcStage,
                    .srcAccessMask = desc.srcAccess,
                    .dstStageMask = desc.dstStage,
                    .dstAccessMask = desc.dstAccess,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored, // not needed
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored, // not needed
                    .buffer = m_Device->getBuffer(handle)->buffer_,
                    .offset = 0,
                    .size = m_Device->getBuffer(handle)->size_,
                });
            }

            vk::DependencyInfo acquireInfo{
                .bufferMemoryBarrierCount = static_cast<uint32_t>(acquireBarriers.size()),
                .pBufferMemoryBarriers = acquireBarriers.data(),
            };
            m_CommandBuffer.pipelineBarrier2(acquireInfo);

            return releaseSubmission;
        }

        std::vector<vk::BufferMemoryBarrier2> releaseBarriers;
        releaseBarriers.reserve(desc.handles.size());

        for (auto& handle : desc.handles)
        {
            auto* buffer = m_Device->getBuffer(handle);
            EASSERT(buffer != nullptr);
            releaseBarriers.push_back(vk::BufferMemoryBarrier2{
                .srcStageMask = desc.srcStage,
                .srcAccessMask = desc.srcAccess,
                .dstStageMask = vk::PipelineStageFlagBits2::eNone, // ignored
                .dstAccessMask = vk::AccessFlagBits2::eNone, // ignored
                .srcQueueFamilyIndex = desc.srcQueue.familyIndex(),
                .dstQueueFamilyIndex = desc.dstQueue.familyIndex(),
                .buffer = buffer->buffer_,
                .offset = 0,
                .size = buffer->size_,
            });
        }

        vk::DependencyInfo releaseInfo{
            .bufferMemoryBarrierCount = static_cast<uint32_t>(releaseBarriers.size()),
            .pBufferMemoryBarriers = releaseBarriers.data(),
        };

        SubmitHandle releaseSubmission = desc.srcQueue.submitImmediateAsync([&](CommandBuffer& cmd) {
            cmd.get().pipelineBarrier2(releaseInfo);
        });

        std::vector<vk::BufferMemoryBarrier2> acquireBarriers;
        acquireBarriers.reserve(desc.handles.size());
        for (auto& handle : desc.handles)
        {
            auto* buffer = m_Device->getBuffer(handle);
            acquireBarriers.push_back(vk::BufferMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eNone, // ignored
                .srcAccessMask = vk::AccessFlagBits2::eNone, // ignored
                .dstStageMask = desc.dstStage,
                .dstAccessMask = desc.dstAccess,
                .srcQueueFamilyIndex = desc.srcQueue.familyIndex(),
                .dstQueueFamilyIndex = desc.dstQueue.familyIndex(),
                .buffer = buffer->buffer_,
                .offset = 0,
                .size = buffer->size_,
            });
        }

        vk::DependencyInfo acquireInfo{
            .bufferMemoryBarrierCount = static_cast<uint32_t>(acquireBarriers.size()),
            .pBufferMemoryBarriers = acquireBarriers.data(),
        };

        m_CommandBuffer.pipelineBarrier2(acquireInfo);

        return releaseSubmission;
    }

    void CommandBuffer::blitImage(TextureHandle srcTexHandle, TextureHandle dstTexHandle)
    {
        EASSERT(m_Device != nullptr);
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_BARRIER)

        auto* srcImage = m_Device->getImage(srcTexHandle);
        EASSERT(srcImage != nullptr);
        auto* dstImage = m_Device->getImage(dstTexHandle);
        EASSERT(dstImage != nullptr);

        vk::Extent2D srcExtent = {srcImage->extent_.width, srcImage->extent_.height};
        vk::Extent2D dstExtent = {dstImage->extent_.width, dstImage->extent_.height};

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

    void CommandBuffer::clearColorImage(TextureHandle texHandle, vk::ClearColorValue color,
                                        vk::ImageAspectFlags aspectMask)
    {
        EASSERT(m_Device != nullptr);
        auto* image = m_Device->getImage(texHandle);
        EASSERT(image != nullptr);

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
        EASSERT(m_Device != nullptr);
        auto* srcBufferObj = m_Device->getBuffer(srcBuffer);
        EASSERT(srcBufferObj != nullptr);
        auto* dstBufferObj = m_Device->getBuffer(dstBuffer);
        EASSERT(dstBufferObj != nullptr);

        m_CommandBuffer.copyBuffer(srcBufferObj->buffer_, dstBufferObj->buffer_, 1, &region);
    }

    void CommandBuffer::copyBufferToImage(BufferHandle buffer, TextureHandle image, std::span<const vk::BufferImageCopy> regions)
    {
        EASSERT(m_Device != nullptr);
        auto* bufferObj = m_Device->getBuffer(buffer);
        EASSERT(bufferObj != nullptr);
        auto* imageObj = m_Device->getImage(image);
        EASSERT(imageObj != nullptr);

        m_CommandBuffer.copyBufferToImage(bufferObj->buffer_, imageObj->image_, vk::ImageLayout::eTransferDstOptimal, regions);
    }

    void CommandBuffer::copyBufferToImage(BufferHandle buffer, TextureHandle image, vk::BufferImageCopy& region)
    {
        copyBufferToImage(buffer, image, {{region}});
    }

    void CommandBuffer::copyBufferToImage2(vk::CopyBufferToImageInfo2& info)
    {
        m_CommandBuffer.copyBufferToImage2(info);
    }

    void CommandBuffer::setViewport(vk::Viewport viewport)
    {
        m_CommandBuffer.setViewport(0, 1, &viewport);
    }

    void CommandBuffer::setScissor(vk::Rect2D scissor)
    {
        m_CommandBuffer.setScissor(0, 1, &scissor);
    }

    void CommandBuffer::beginRendering(vk::RenderingInfo& renderingInfo)
    {
        m_CommandBuffer.beginRendering(renderingInfo);
    }

    void CommandBuffer::endRendering()
    {
        m_CommandBuffer.endRendering();
    }

    void CommandBuffer::bindComputePipeline(ComputePipelineHandle pipelineHandle)
    {
        EASSERT(m_Device != nullptr);
        auto* pipeline = m_Device->getComputePipeline(pipelineHandle);
        EASSERT(pipeline != nullptr);

        m_CommandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->handle);
    }

    void CommandBuffer::bindGraphicsPipeline(GraphicsPipelineHandle pipelineHandle)
    {
        EASSERT(m_Device != nullptr);
        auto* pipeline = m_Device->getGraphicsPipeline(pipelineHandle);
        EASSERT(pipeline != nullptr);

        m_CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->handle);
    }

    void CommandBuffer::bindDescriptorSets(vk::PipelineBindPoint bindPoint, PipelineLayoutHandle pipelineLayout,
                                           uint32_t firstSet, std::span<const vk::DescriptorSet> descriptorSets)
    {
        EASSERT(m_Device != nullptr);
        auto* layout = m_Device->getPipelineLayout(pipelineLayout);
        EASSERT(layout != nullptr);

        m_CommandBuffer.bindDescriptorSets(bindPoint, layout->layout, firstSet, descriptorSets, nullptr);
    }

    void CommandBuffer::bindDescriptorSetsBindless(vk::PipelineBindPoint bindPoint)
    {
        EASSERT(m_Device != nullptr);
        EASSERT(m_Device->bindlessDescriptorSet());
        PipelineLayout* layout = nullptr;

        switch (bindPoint)
        {
            case vk::PipelineBindPoint::eGraphics:
                layout = m_Device->getPipelineLayout(m_Device->bindlessGraphicsPipelineLayout());
                break;
            case vk::PipelineBindPoint::eCompute:
                layout = m_Device->getPipelineLayout(m_Device->bindlessComputePipelineLayout());
                break;
            default:
                layout = nullptr;
        }
        EASSERT(layout);
        m_CommandBuffer.bindDescriptorSets(bindPoint, layout->layout, 0,
                                           m_Device->bindlessDescriptorSet(), nullptr);
    }

    void CommandBuffer::pushConstants(PipelineLayoutHandle pipelineLayout, vk::ShaderStageFlags stages,
                                      uint32_t offset, uint32_t size, const void* data)
    {
        EASSERT(m_Device != nullptr);
        auto* layout = m_Device->getPipelineLayout(pipelineLayout);
        EASSERT(layout != nullptr);

        m_CommandBuffer.pushConstants(layout->layout, stages, offset, size, data);
    }

    void CommandBuffer::bindIndexBuffer(BufferHandle buffer, uint32_t offset, vk::IndexType indexType)
    {
        EASSERT(m_Device != nullptr);
        auto* rawBuf = m_Device->getBuffer(buffer);
        EASSERT(rawBuf != nullptr);

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

    CommandBuffer::CommandBuffer(Device* device, vk::CommandBuffer commandBuffer)
        :
        m_Device(device),
        m_CommandBuffer(commandBuffer)
    {}
}
