#include "QueueSubmitBuilder.h"

#include "Commands.h"
#include "Logging/Assert.h"

namespace enger
{
    void QueueSubmitBuilder::waitBinary(vk::Semaphore semaphore, vk::PipelineStageFlags2 stage)
    {
        EASSERT(waitSemaphoreCount_ < kMaxSemaphores, "Too many wait semaphores");
        waitSemaphores_[waitSemaphoreCount_++] = vk::SemaphoreSubmitInfo{
            .semaphore = semaphore,
            .stageMask = stage
        };
    }

    void QueueSubmitBuilder::signalBinary(vk::Semaphore semaphore, vk::PipelineStageFlags2 stage)
    {
        EASSERT(signalSemaphoreCount_ < kMaxSemaphores, "Too many signal semaphores");
        signalSemaphores_[signalSemaphoreCount_++] = vk::SemaphoreSubmitInfo{
            .semaphore = semaphore,
            .stageMask = stage
        };
    }

    void QueueSubmitBuilder::waitTimeline(vk::Semaphore semaphore, uint64_t waitValue, vk::PipelineStageFlags2 stage)
    {
        EASSERT(waitSemaphoreCount_ < kMaxSemaphores, "Too many wait semaphores");
        waitSemaphores_[waitSemaphoreCount_++] = vk::SemaphoreSubmitInfo{
            .semaphore = semaphore,
            .value = waitValue,
            .stageMask = stage,
        };
    }

    void QueueSubmitBuilder::waitTimeline(SubmitHandle handle, vk::PipelineStageFlags2 stage)
    {
        waitTimeline(handle.timelineSemaphore, handle.value, stage);
    }

    void QueueSubmitBuilder::signalTimeline(vk::Semaphore semaphore, uint64_t signalValue, vk::PipelineStageFlags2 stage)
    {
        EASSERT(signalSemaphoreCount_ < kMaxSemaphores, "Too many signal semaphores");
        signalSemaphores_[signalSemaphoreCount_++] = vk::SemaphoreSubmitInfo{
            .semaphore = semaphore,
            .value = signalValue,
            .stageMask = stage,
        };
    }

    void QueueSubmitBuilder::signalTimeline(SubmitHandle handle, vk::PipelineStageFlags2 stage)
    {
        signalTimeline(handle.timelineSemaphore, handle.value, stage);
    }

    void QueueSubmitBuilder::addCmd(CommandBuffer& commandBuffer)
    {
        EASSERT(commandBufferCount_ < kMaxCommandBuffers, "Too many command buffers");
        commandBuffers_[commandBufferCount_++] = vk::CommandBufferSubmitInfo{
            .commandBuffer = commandBuffer.get()
        };
    }

    vk::SubmitInfo2 QueueSubmitBuilder::build() const
    {
        return vk::SubmitInfo2{
            .waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreCount_),
            .pWaitSemaphoreInfos = waitSemaphores_.data(),
            .commandBufferInfoCount = static_cast<uint32_t>(commandBufferCount_),
            .pCommandBufferInfos = commandBuffers_.data(),
            .signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreCount_),
            .pSignalSemaphoreInfos = signalSemaphores_.data(),
        };
    }

    void QueueSubmitBuilder::clear()
    {
        waitSemaphoreCount_ = 0;
        signalSemaphoreCount_ = 0;
        commandBufferCount_ = 0;
    }
}
