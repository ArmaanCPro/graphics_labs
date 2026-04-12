#pragma once

#include "vk.h"

namespace enger
{
    class Allocator
    {
    public:
        // TODO this is dirty, the default constructor is invalid state until property initialized
        Allocator() = default;

        Allocator(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);

        ~Allocator();

        void init(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);

        [[nodiscard]] VmaAllocation createBuffer(vk::BufferCreateInfo &bufferCI, vk::Buffer &buffer,
                                                 vk::MemoryPropertyFlags memFlags, bool coherent, void* mappedPtr = nullptr);

        /// This function handles the Vulkan creation of an image AND allocation.
        [[nodiscard]] VmaAllocation createImage(vk::ImageCreateInfo &imageCI, vk::Image &image);

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
