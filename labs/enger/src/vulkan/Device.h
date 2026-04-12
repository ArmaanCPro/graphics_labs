#pragma once

#include <functional>

#include "GpuResourceTypes.h"
#include "vk.h"

#include "Resources.h"
#include "Pipeline.h"

#include "Allocator.h"
#include "Commands.h"
#include "Descriptors.h"

#include "Queue.h"


namespace enger
{
    struct DescriptorSetLayoutDesc;

    class Device
    {
    public:
        /// Requires surface for presentation. Headless is not currently supported.
        explicit Device(vk::Instance instance, vk::SurfaceKHR surface, std::span<const char*> deviceExtensions, bool useBindless = true);

        ~Device();

        [[nodiscard]] vk::PhysicalDevice physicalDevice()
        {
            return m_PhysicalDevice;
        }

        [[nodiscard]] vk::Device device()
        {
            return *m_Device;
        }

        [[nodiscard]] Queue& graphicsQueue()
        {
            return m_GraphicsQueue;
        }

        [[nodiscard]] const Allocator& allocator() const
        {
            return m_Allocator;
        }

        void waitSemaphores(std::span<const vk::Semaphore> semaphores, std::span<const uint64_t> waitValues,
                            uint64_t timeout = std::numeric_limits<uint64_t>::max());

        Holder<ComputePipelineHandle> createComputePipeline(ComputePipelineDesc desc, Queue* queue,
                                                            std::string_view debugName = "");

        Holder<GraphicsPipelineHandle> createGraphicsPipeline(GraphicsPipelineDesc desc, Queue* queue,
                                                              std::string_view debugName = "");

        Holder<PipelineLayoutHandle> createPipelineLayout(PipelineLayoutDesc desc, Queue* queue,
                                                          std::string_view debugName = "");

        Holder<TextureHandle> createTexture(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage,
                                            Queue* queue, std::string_view debugName = "");

        Holder<BufferHandle> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                          vk::MemoryPropertyFlags memFlags,
                                          Queue* queue, std::string_view debugName = "");

        Holder<DescriptorSetLayoutHandle> createDescriptorSetLayout(DescriptorSetLayoutDesc desc, Queue* queue,
                                                                    std::string_view debugName = "");

        Holder<ShaderModuleHandle> createShaderModule(std::span<const uint32_t> code, Queue* queue,
                                                      std::string_view debugName = "");

        void destroyComputePipeline(ComputePipelineHandle handle, Queue* queue);

        void destroyGraphicsPipeline(GraphicsPipelineHandle handle, Queue* queue);

        void destroyPipelineLayout(PipelineLayoutHandle handle, Queue* queue);

        void destroyTexture(TextureHandle handle, Queue* queue);

        void destroyBuffer(BufferHandle handle, Queue* queue);

        void destroyDescriptorSetLayout(DescriptorSetLayoutHandle handle, Queue* queue);

        void destroyShaderModule(ShaderModuleHandle handle, Queue* queue);

        UniqueCommandPool createUniqueCommandPool(CommandPoolFlags flags, uint32_t queueFamilyIndex,
                                                  std::string_view debugName = "");

        std::vector<UniqueCommandPool> createUniqueCommandPools(CommandPoolFlags flags, uint32_t queueFamilyIndex,
                                                                uint32_t count, std::string_view debugName = "");

        CommandBuffer allocateCommandBuffer(UniqueCommandPool& commandPool, CommandBufferLevel level,
                                            std::string_view debugName = "");

        std::vector<CommandBuffer> allocateCommandBuffers(UniqueCommandPool& commandPool, CommandBufferLevel level,
                                                          uint32_t count, std::string_view debugName = "");

        // useful for swapchain. Resource deallocation is not automatic.
        [[nodiscard]] TextureHandle addTextureToPool(VulkanImage&& image);

        void removeTextureFromPool(TextureHandle handle);

        // TODO consider a better API to get raw objects from Pools
        [[nodiscard]] VulkanImage* getImage(TextureHandle handle)
        {
            return m_TexturePool.get(handle);
        };

        [[nodiscard]] VulkanBuffer* getBuffer(BufferHandle handle)
        {
            return m_BufferPool.get(handle);
        };

        [[nodiscard]] vk::DescriptorSetLayout* getDescriptorSetLayout(DescriptorSetLayoutHandle handle)
        {
            return m_DescriptorSetLayoutPool.get(handle);
        };

        [[nodiscard]] Pipeline* getComputePipeline(ComputePipelineHandle handle)
        {
            return m_ComputePipelinePool.get(handle);
        };

        [[nodiscard]] Pipeline* getGraphicsPipeline(GraphicsPipelineHandle handle)
        {
            return m_GraphicsPipelinePool.get(handle);
        };

        [[nodiscard]] PipelineLayout* getPipelineLayout(PipelineLayoutHandle handle)
        {
            return m_PipelineLayoutPool.get(handle);
        };

        [[nodiscard]] DescriptorSetLayoutHandle bindlessDescriptorSetLayout()
        {
            return m_BindlessLayoutHandle;
        }

        [[nodiscard]] const vk::DescriptorSet& bindlessDescriptorSet() const
        {
            return *m_GlobalDescriptorSet;
        }

    private:
        vk::PhysicalDevice m_PhysicalDevice;
        vk::UniqueDevice m_Device;

        // TODO: dependency inject this via polymorphism in ctor, maybe
        Allocator m_Allocator;

        /// As of now, the Graphics Queue is the only one guaranteed to exist for our program.
        /// Therefore, whenever a Queue is not specified, it defaults to this queue for any operations such as
        /// resource creation or destruction synchronization.
        Queue m_GraphicsQueue;

        Pool<ComputePipelineTag, Pipeline> m_ComputePipelinePool;
        Pool<GraphicsPipelineTag, Pipeline> m_GraphicsPipelinePool;
        Pool<PipelineLayoutTag, PipelineLayout> m_PipelineLayoutPool;
        Pool<TextureTag, VulkanImage> m_TexturePool;
        Pool<BufferTag, VulkanBuffer> m_BufferPool;
        Pool<DescriptorSetLayoutTag, vk::DescriptorSetLayout> m_DescriptorSetLayoutPool;
        Pool<ShaderModuleTag, vk::ShaderModule> m_ShaderModulePool;

        // BINDLESS
        bool m_UseBindless;
        Holder<DescriptorSetLayoutHandle> m_BindlessLayoutHandle;
        vk::UniqueDescriptorPool m_BindlessPool;
        vk::UniqueDescriptorSet  m_GlobalDescriptorSet;

        void initBindlessDescriptors();

        // consider adding this on resource deletion, and writing VK_NULL_HANLDE/nullptr for robustness2
        void updateBindlessStorageImage(uint32_t index, vk::ImageView view);
        void updateBindlessSampledImage(uint32_t index, vk::ImageView view);
    };
}
