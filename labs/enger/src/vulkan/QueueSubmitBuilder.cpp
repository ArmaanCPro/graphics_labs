#include "QueueSubmitBuilder.h"

#include "Commands.h"

namespace enger
{
    void QueueSubmitBuilder::waitBinary(vk::Semaphore semaphore, vk::PipelineStageFlags2 stage)
    {
        assert(waitSemaphoreCount_ + signalSemaphoreCount_ < kMaxSemaphores);
        semaphores_[waitSemaphoreCount_++ + signalSemaphoreCount_] = vk::SemaphoreSubmitInfo{
            .semaphore = semaphore,
            .stageMask = stage
        };
    }

    void QueueSubmitBuilder::signalBinary(vk::Semaphore semaphore, vk::PipelineStageFlags2 stage)
    {
        assert(waitSemaphoreCount_ + signalSemaphoreCount_< kMaxSemaphores);
        semaphores_[waitSemaphoreCount_ + signalSemaphoreCount_++] = vk::SemaphoreSubmitInfo{
            .semaphore = semaphore,
            .stageMask = stage
        };
    }

    void QueueSubmitBuilder::waitTimeline(vk::Semaphore semaphore, uint64_t waitValue, vk::PipelineStageFlags2 stage)
    {
        assert(waitSemaphoreCount_ + signalSemaphoreCount_ < kMaxSemaphores);
        semaphores_[waitSemaphoreCount_++ + signalSemaphoreCount_] = vk::SemaphoreSubmitInfo{
            .semaphore = semaphore,
            .value = waitValue,
            .stageMask = stage,
        };
    }

    void QueueSubmitBuilder::signalTimeline(vk::Semaphore semaphore, uint64_t signalValue, vk::PipelineStageFlags2 stage)
    {
        assert(waitSemaphoreCount_ + signalSemaphoreCount_ < kMaxSemaphores);
        semaphores_[waitSemaphoreCount_ + signalSemaphoreCount_++] = vk::SemaphoreSubmitInfo{
            .semaphore = semaphore,
            .value = signalValue,
            .stageMask = stage,
        };
    }

    void QueueSubmitBuilder::addCmd(CommandBuffer commandBuffer)
    {
        assert(commandBufferCount_ < kMaxCommandBuffers);
        commandBuffers_[commandBufferCount_++] = vk::CommandBufferSubmitInfo{
            .commandBuffer = commandBuffer.get()
        };
    }

    vk::SubmitInfo2 QueueSubmitBuilder::build() const
    {
        return vk::SubmitInfo2{
            .waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreCount_),
            .pWaitSemaphoreInfos = semaphores_.data(),
            .commandBufferInfoCount = static_cast<uint32_t>(commandBufferCount_),
            .pCommandBufferInfos = commandBuffers_.data(),
            .signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreCount_),
            .pSignalSemaphoreInfos = semaphores_.data() + waitSemaphoreCount_,
        };
    }
}
