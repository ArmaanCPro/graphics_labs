#pragma once

#include <functional>

#include "GpuResourceTypes.h"
#include "vk.h"

#include "Resources.h"
#include "Pipeline.h"

#include "Allocator.h"

namespace enger
{
    /// Represents a GPU moment.
    /// Easily defined using timeline semaphores:
    ///     | A timeline semaphore value maps directly onto a SubmitHandle value.
    /// Used for synchronization and deletion.
    /// I.e., a resource will only be released after the current submit handle is >= the resource's designated submit handle.
    using SubmitHandle = uint64_t;

    struct Queue
    {
        vk::Queue queue;
        uint32_t index;
    };

    class Device
    {
    public:
        /// Requires surface for presentation. Headless is not currently supported.
        explicit Device(vk::Instance instance, vk::SurfaceKHR surface, std::span<const char*> deviceExtensions);
        ~Device();

        [[nodiscard]] vk::PhysicalDevice physicalDevice() { return m_PhysicalDevice; }
        [[nodiscard]] vk::Device device() { return *m_Device; }

        [[nodiscard]] Queue graphicsQueue() { return m_GraphicsQueue; }

        [[nodiscard]] SubmitHandle currentSubmitCounter() const { return m_CurrentSubmitCounter; }
        [[nodiscard]] vk::Semaphore timelineSemaphore() const { return *m_TimelineSemaphore; }

        Holder<TextureHandle> createTexture(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, std::string_view debugName = "");

        void destroyComputePipeline(ComputePipelineHandle handle);
        void destroyTexture(TextureHandle handle);

        /// TODO change type of submitInfo to be RHI agnostic
        void submitGraphics(vk::SubmitInfo2 submitInfo);

        // Should be called once per frame.
        void flushDeletionQueue();
        // for debugging
        void forceDeletionQueueFlush();

    private:
        vk::PhysicalDevice m_PhysicalDevice;
        vk::UniqueDevice m_Device;

        Queue m_GraphicsQueue;

        /// the timeline semaphore and submit counter are queue-specific (i.e., one for graphics, one for async)
        /// consider moving them to a struct once we have more queues (i.e., async compute)
        /// then I would need to pass in a vk::Timeline handle to a DeferredDeletionTask and a destroy(device, handle) and holder<>
        vk::UniqueSemaphore m_TimelineSemaphore;
        /// Tracks the current submit handle (CPU side, not the actual GPU state)
        SubmitHandle m_CurrentSubmitCounter = {0};

        // TODO: dependency inject this via polymorphism in ctor, maybe
        Allocator m_Allocator;

        Pool<ComputePipelineTag, Pipeline> m_ComputePipelinePool;
        Pool<TextureTag, VulkanImage> m_TexturePool;

        struct DeferredDeletionTask
        {
            std::function<void()> func;
            SubmitHandle submitValue;
        };
        std::vector<DeferredDeletionTask> m_DeletionQueue;
    };
}
