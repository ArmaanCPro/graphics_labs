#pragma once

#include <functional>

#include "GpuResourceTypes.h"
#include "vk.h"

#include "Resources.h"
#include "Pipeline.h"

#include "Allocator.h"
#include "Commands.h"


namespace enger
{
    struct DescriptorSetLayoutDesc;

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

        void waitSemaphores(std::span<vk::Semaphore> semaphores, std::span<uint64_t> waitValues, uint64_t timeout);

        Holder<TextureHandle> createTexture(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, std::string_view debugName = "");

        Holder<DescriptorSetLayoutHandle> createDescriptorSetLayout(DescriptorSetLayoutDesc desc, std::string_view debugName = "");

        void destroyComputePipeline(ComputePipelineHandle handle);
        void destroyTexture(TextureHandle handle);
        void destroyDescriptorSetLayout(DescriptorSetLayoutHandle handle);

        /// TODO change type of submitInfo to be RHI agnostic
        void submitGraphics(vk::SubmitInfo2 submitInfo);

        UniqueCommandPool createUniqueCommandPool(CommandPoolFlags flags, uint32_t queueFamilyIndex, std::string_view debugName = "");
        std::vector<UniqueCommandPool> createUniqueCommandPools(CommandPoolFlags flags, uint32_t queueFamilyIndex, uint32_t count, std::string_view debugName = "");
        CommandBuffer allocateCommandBuffer(UniqueCommandPool& commandPool, CommandBufferLevel level, std::string_view debugName = "");
        std::vector<CommandBuffer> allocateCommandBuffers(UniqueCommandPool& commandPool, CommandBufferLevel level, uint32_t count, std::string_view debugName = "");

        // Should be called once per frame.
        void flushDeletionQueue();
        // for debugging
        void forceDeletionQueueFlush();

        // useful for swapchain. Resource deallocation is not automatic.
        [[nodiscard]] TextureHandle addTextureToPool(VulkanImage&& image);
        void removeTextureFromPool(TextureHandle handle);

        // TODO consider a better API to get raw objects from Pools
        [[nodiscard]] VulkanImage* getImage(TextureHandle handle) { return m_TexturePool.get(handle); };
        [[nodiscard]] vk::DescriptorSetLayout* getDescriptorSetLayout(DescriptorSetLayoutHandle handle) { return m_DescriptorSetLayoutPool.get(handle); };

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

        struct DeferredDeletionTask
        {
            std::function<void()> func;
            SubmitHandle submitValue;
        };
        std::vector<DeferredDeletionTask> m_DeletionQueue;

        Pool<ComputePipelineTag, Pipeline> m_ComputePipelinePool;
        Pool<TextureTag, VulkanImage> m_TexturePool;
        Pool<DescriptorSetLayoutTag, vk::DescriptorSetLayout> m_DescriptorSetLayoutPool;
    };
}
