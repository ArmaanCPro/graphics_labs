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

        VmaAllocation allocateBuffer() { return {}; }
        /// This function handles the Vulkan creation of an image AND allocation.
        VmaAllocation createImage(vk::ImageCreateInfo& imageCI, vk::Image& image);

        /// This function solely deallocates an allocation.
        void free(VmaAllocation alloc);

        /// This function calls vmaDestroyImage() internally.
        /// Does both Vulkan destruction AND deallocation of the image.
        void destroyImage(VmaAllocation alloc, vk::Image image);

    private:
        VmaAllocator m_Allocator{};

        VmaVulkanFunctions m_VulkanFunctions{};
    };
}