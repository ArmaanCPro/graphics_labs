#include "Queue.h"

#include "Device.h"
#include "Commands.h"

#include <format>

#include "Logging/Assert.h"

namespace enger
{
    void DeferredDeletionQueue::push(std::function<void()> func, SubmitHandle submitValue)
    {
        m_Tasks.push_back({std::move(func), submitValue});
    }

    void DeferredDeletionQueue::flush(vk::Device device, vk::Semaphore timeline)
    {
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_DESTROY);
        uint64_t gpuSubmitValue = vkCheck(device.getSemaphoreCounterValue(timeline));
        std::erase_if(m_Tasks, [&](const auto& task) {
            if (task.submitValue <= gpuSubmitValue)
            {
                task.func();
                return true;
            }
            return false;
        });
    }

    void DeferredDeletionQueue::forceFlush()
    {
        for (auto& task : m_Tasks)
        {
            task.func();
        }
        m_Tasks.clear();
    }

    Queue::Queue(Device* device, uint32_t familyIndex, std::string_view debugName)
        :
        m_Device(device),
        m_Queue(device->device().getQueue(familyIndex, 0)),
        m_FamilyIndex(familyIndex),
        m_ImmediateCmdPool(device->createUniqueCommandPool(CommandPoolFlags::ResetCommandBuffer, familyIndex,
                                                           debugName.empty()
                                                               ? ""
                                                               : std::format("{}_{}", debugName, "_ImmediateCmdPool")))
    {
        vk::SemaphoreTypeCreateInfo semaphoreTypeCI{
            .semaphoreType = vk::SemaphoreType::eTimeline,
            .initialValue = 0,
        };
        vk::SemaphoreCreateInfo semaphoreCI{
            .pNext = &semaphoreTypeCI,
        };

        m_TimelineSemaphore = vkCheck(m_Device->device().createSemaphoreUnique(semaphoreCI));
        m_CurrentSubmitCounter = SubmitHandle{
            .value = 0,
            .timelineSemaphore = *m_TimelineSemaphore,
        };

        if (!debugName.empty())
        {
            setDebugName(device->device(), m_Queue, debugName);
            setDebugName(device->device(), *m_TimelineSemaphore,
                         std::format("{}_{}", debugName, "TimelineSemaphore"));
        }

        m_ImmediateCmdBuffer = m_Device->allocateCommandBuffer(m_ImmediateCmdPool,
                                                               CommandBufferLevel::Primary,
                                                               debugName.empty()
                                                                   ? ""
                                                                   : std::format(
                                                                       "{}_{}", debugName, "ImmediateCommandBuffer"));
    }

    Queue::~Queue()
    {
        vkCheck(m_Device->device().waitIdle());
        forceDeletionQueueFlush();
    }

    SubmitHandle Queue::submit(vk::SubmitInfo2 submitInfo)
    {
        ENGER_PROFILE_FUNCTION()
        ++m_CurrentSubmitCounter;
        // Signal the timeline semaphore
        std::vector<vk::SemaphoreSubmitInfo> signalSemaphores(
            submitInfo.pSignalSemaphoreInfos,
            submitInfo.pSignalSemaphoreInfos + submitInfo.signalSemaphoreInfoCount
        );
        signalSemaphores.push_back(vk::SemaphoreSubmitInfo{
            .semaphore = *m_TimelineSemaphore,
            .value = m_CurrentSubmitCounter,
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands
        });
        submitInfo.signalSemaphoreInfoCount += 1;
        submitInfo.pSignalSemaphoreInfos = signalSemaphores.data();

        vkCheck(m_Queue.submit2(submitInfo, nullptr));
        return m_CurrentSubmitCounter;
    }

    SubmitHandle Queue::submitImmediateAsync(std::function<void(CommandBuffer&)> func,
                                             std::optional<vk::SubmitInfo2> submitInfo)
    {
        ENGER_PROFILE_FUNCTION();
        wait(m_CurrentSubmitCounter);
        // if we used an Arena style immediate command buffer ring, then we wouldn't need this unless ALL arenas are occupied
        m_ImmediateCmdBuffer.reset();
        m_ImmediateCmdBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        func(m_ImmediateCmdBuffer);
        m_ImmediateCmdBuffer.end();

        vk::CommandBufferSubmitInfo cmdInfo{
            .commandBuffer = m_ImmediateCmdBuffer.get(),
        };

        if (submitInfo.has_value())
        {
            if (submitInfo.value().commandBufferInfoCount == 0)
            {
                submitInfo.value().commandBufferInfoCount = 1;
                submitInfo.value().pCommandBufferInfos = &cmdInfo;
                return submit(submitInfo.value());
            }
            std::vector<vk::CommandBufferSubmitInfo> infos(submitInfo.value().pCommandBufferInfos,
                                                           submitInfo.value().pCommandBufferInfos + submitInfo.value().
                                                           commandBufferInfoCount);
            infos.push_back(cmdInfo);
            submitInfo.value().pCommandBufferInfos = infos.data();
            submitInfo.value().commandBufferInfoCount += 1;
            return submit(submitInfo.value());
        }

        vk::SubmitInfo2 si{
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdInfo,
        };
        return submit(si);
    }

    void Queue::submitImmediate(std::function<void(CommandBuffer&)> func, uint64_t timeout)
    {
        ENGER_PROFILE_FUNCTION();
        wait(submitImmediateAsync(std::move(func)), timeout);
    }

    void Queue::flushDeletionQueue()
    {
        m_DeletionQueue.flush(m_Device->device(), *m_TimelineSemaphore);
    }

    void Queue::forceDeletionQueueFlush()
    {
        m_DeletionQueue.forceFlush();
    }

    void Queue::waitIdle()
    {
        vkCheck(m_Queue.waitIdle());
    }

    void Queue::wait(SubmitHandle handle, uint64_t timeout)
    {
        std::array<vk::Semaphore, 1> semaphores{handle.timelineSemaphore};
        std::array<uint64_t, 1> values{handle.value};
        m_Device->waitSemaphores(semaphores, values, timeout);
    }

    void Queue::deferredDestroy(std::function<void()> func)
    {
        m_DeletionQueue.push(std::move(func), m_CurrentSubmitCounter);
    }

    void Queue::uploadTexture2DData(TextureHandle handle, std::span<const TextureSubresource> data,
                                    const vk::Extent3D& dimensions, uint32_t mipLevels,
                                    [[maybe_unused]] uint32_t arrayLayers, [[maybe_unused]] vk::Format imageFormat, bool genMips)
    {
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_CREATE);
        size_t size = 0;
        for (const auto& sub : data)
            size += sub.size;
        if (size == 0)
            size = dimensions.width * dimensions.height * dimensions.depth * (findBppFromFormat(imageFormat) / 8);

        auto stagingHandle = m_Device->createBuffer(size,
                                                    vk::BufferUsageFlagBits::eTransferSrc,
                                                    vk::MemoryPropertyFlagBits::eHostVisible |
                                                    vk::MemoryPropertyFlagBits::eHostCoherent,
                                                    this, "StagingBufferForImage");

        // TODO move buffer subdata to Device as an internal function
        auto* staging = m_Device->getBuffer(stagingHandle);
        EASSERT(staging && staging->mappedMemory_);

        submitImmediate([&](CommandBuffer& cmd) {
            cmd.transitionImage(handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

            std::vector<vk::BufferImageCopy2> regions;
            regions.reserve(data.size());
            uint32_t currentOffset = 0;
            for (auto& subresource : data)
            {
                staging->bufferSubData(m_Device->allocator(), currentOffset, subresource.size, subresource.data);
                regions.push_back(vk::BufferImageCopy2{
                    .bufferOffset = currentOffset,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,

                    .imageSubresource = vk::ImageSubresourceLayers{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = subresource.mipLevel,
                        .baseArrayLayer = subresource.arrayLayer,
                        .layerCount = 1,
                    },
                    .imageOffset = {0, 0, 0},
                    .imageExtent = subresource.extent,
                });
                currentOffset += static_cast<uint32_t>(subresource.size);
            }

            vk::CopyBufferToImageInfo2 info{
                .srcBuffer = m_Device->getBuffer(stagingHandle)->buffer_,
                .dstImage = m_Device->getImage(handle)->image_,
                .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                .regionCount = static_cast<uint32_t>(regions.size()),
                .pRegions = regions.data(),
            };
            cmd.copyBufferToImage2(info);
            if (genMips)
            {
                generateMipmaps(cmd, handle, {dimensions.width, dimensions.height}, mipLevels);
            }
            else
            {
                cmd.transitionImage(handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral);
            }
        });
    }

    void Queue::generateMipmaps(CommandBuffer& cmd, TextureHandle handle, vk::Extent2D dimensions, uint32_t mipLevels)
    {
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_CREATE);
        for (uint32_t mip = 0; mip < mipLevels; ++mip)
        {
            vk::Extent2D halfSize = dimensions;
            halfSize.width /= 2;
            halfSize.height /= 2;

            // TODO expand cmd.transitionImage to support mips?
            vk::ImageMemoryBarrier2 barrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .dstAccessMask = vk::AccessFlagBits2::eTransferRead,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                .image = m_Device->getImage(handle)->image_,

                .subresourceRange = vk::ImageSubresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = mip,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            vk::DependencyInfo dependencyInfo{
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier,
            };

            cmd.get().pipelineBarrier2(dependencyInfo);

            if (mip < mipLevels - 1)
            {
                // could also extend cmd.blitImage() here to support mips
                vk::ImageBlit2 blitRegion{};
                blitRegion.srcOffsets[1].x = dimensions.width;
                blitRegion.srcOffsets[1].y = dimensions.height;
                blitRegion.srcOffsets[1].z = 1;

                blitRegion.dstOffsets[1].x = halfSize.width;
                blitRegion.dstOffsets[1].y = halfSize.height;
                blitRegion.dstOffsets[1].z = 1;

                blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                blitRegion.srcSubresource.baseArrayLayer = 0;
                blitRegion.srcSubresource.layerCount = 1;
                blitRegion.srcSubresource.mipLevel = mip;

                blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                blitRegion.dstSubresource.baseArrayLayer = 0;
                blitRegion.dstSubresource.layerCount = 1;
                blitRegion.dstSubresource.mipLevel = mip + 1;

                vk::BlitImageInfo2 blitInfo{
                    .srcImage = m_Device->getImage(handle)->image_,
                    .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
                    .dstImage = m_Device->getImage(handle)->image_,
                    .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                    .regionCount = 1,
                    .pRegions = &blitRegion,
                    .filter = vk::Filter::eLinear,
                };

                cmd.get().blitImage2(blitInfo);

                dimensions = halfSize;
            }
        }
        cmd.transitionImage(handle, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);
    }
}
