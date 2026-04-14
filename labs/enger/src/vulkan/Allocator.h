#pragma once

#include "GpuResourceTypes.h"
#include "vk.h"

namespace enger
{
    struct VulkanBuffer;

    class Allocator
    {
    public:
        // TODO this is dirty, the default constructor is invalid state until property initialized
        Allocator() = default;
        Allocator(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);
        ~Allocator();

        Allocator(const Allocator&) = delete;
        Allocator& operator=(const Allocator&) = delete;
        Allocator(Allocator&&) = default;
        Allocator& operator=(Allocator&&) = default;

        void init(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);

        [[nodiscard]] VulkanBuffer createBuffer(vk::BufferCreateInfo &bufferCI,
                                                vk::MemoryPropertyFlags memFlags);

        /// This function handles the Vulkan creation of an image AND allocation.
        [[nodiscard]] VulkanImage createImage(vk::ImageCreateInfo &imageCI, vk::MemoryPropertyFlags memFlags);

        /// This function solely deallocates an allocation.
        void free(VmaAllocation alloc);

        void destroyBuffer(VmaAllocation alloc, vk::Buffer buffer);

        /// This function calls vmaDestroyImage() internally.
        /// Does both Vulkan destruction AND deallocation of the image.
        void destroyImage(VmaAllocation alloc, vk::Image image);

        [[nodiscard]] const VmaAllocator &allocator() const
        {
            return m_Allocator;
        }

    private:
        VmaAllocator m_Allocator{};

        VmaVulkanFunctions m_VulkanFunctions{};
    };
}
