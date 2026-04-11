#pragma once

#include <functional>

#include "Commands.h"
#include "vk.h"

namespace enger
{
    // forward decl
    class Device;

    /// Represents a GPU moment.
    /// Easily defined using timeline semaphores:
    ///     | A timeline semaphore value maps directly onto a SubmitHandle value.
    /// Used for synchronization and deletion.
    /// I.e., a resource will only be released after the current submit handle is >= the resource's designated submit handle.
    using SubmitHandle = uint64_t;

    /// This, for now, is an internal class. Shouldn't be used by clients, but it should be relatively easy to refactor for that.
    /// Therefore, it isn't private currently.
    class DeferredDeletionQueue
    {
    public:
        void push(std::function<void()> func, SubmitHandle submitValue);
        void flush(vk::Device device, vk::Semaphore timeline);

        // For debug purposes
        void forceFlush();

    private:
        struct Task
        {
            std::function<void()> func;
            SubmitHandle submitValue;
        };
        std::vector<Task> m_Tasks;
    };

    /// This is the standard class for operating on a Queue.
    /// This refactored logic previously inside Device.
    /// Device should always own this class, but it is safe to take references to an instance from Device.
    class Queue
    {
    public:
        Queue() = default;
        Queue(Device* device, uint32_t familyIndex, std::string_view debugName = "");
        Queue(const Queue&) = delete;
        Queue& operator=(const Queue&) = delete;
        Queue(Queue&&) = default;
        Queue& operator=(Queue&&) = default;
        ~Queue();

        [[nodiscard]] SubmitHandle submit(vk::SubmitInfo2 submitInfo);

        void submitImmediate(std::function<void(CommandBuffer&)> func);

        void flushDeletionQueue();
        void wait(SubmitHandle handle, uint64_t timeout = std::numeric_limits<uint64_t>::max());
        void waitIdle();
        void forceDeletionQueueFlush();

        void deferredDestroy(std::function<void()> func);

        [[nodiscard]] vk::Semaphore timelineSemaphore() const { return *m_TimelineSemaphore; };
        [[nodiscard]] uint32_t familyIndex() const { return m_FamilyIndex; };
        [[nodiscard]] vk::Queue queue() const { return m_Queue; }

    private:
        Device* m_Device;
        vk::Queue m_Queue;
        uint32_t m_FamilyIndex;
        vk::UniqueSemaphore m_TimelineSemaphore;
        SubmitHandle m_CurrentSubmitCounter = {0};
        DeferredDeletionQueue m_DeletionQueue;

        // Immediate submits
        UniqueCommandPool m_ImmediateCmdPool;
        CommandBuffer m_ImmediateCmdBuffer;
    };
}