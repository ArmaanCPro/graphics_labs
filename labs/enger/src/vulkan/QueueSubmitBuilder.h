#pragma once

#include "vk.h"

namespace enger
{
    class CommandBuffer;

    struct QueueSubmitBuilder
    {
        void waitBinary(vk::Semaphore semaphore, vk::PipelineStageFlags2 stage);
        void signalBinary(vk::Semaphore semaphore, vk::PipelineStageFlags2 stage);
        void waitTimeline(vk::Semaphore semaphore, uint64_t waitValue, vk::PipelineStageFlags2 stage);
        void signalTimeline(vk::Semaphore semaphore, uint64_t signalValue, vk::PipelineStageFlags2 stage);
        void addCmd(CommandBuffer commandBuffer);

        [[nodiscard]] vk::SubmitInfo2 build() const;

    private:
        static constexpr size_t kMaxSemaphores = 8;
        std::array<vk::SemaphoreSubmitInfo, kMaxSemaphores> semaphores_;
        size_t waitSemaphoreCount_ = 0;
        size_t signalSemaphoreCount_ = 0;

        static constexpr size_t kMaxCommandBuffers = 4;
        std::array<vk::CommandBufferSubmitInfo, kMaxCommandBuffers> commandBuffers_;
        size_t commandBufferCount_ = 0;
    };
}
