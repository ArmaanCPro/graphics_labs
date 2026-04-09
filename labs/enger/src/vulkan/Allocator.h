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
        VmaAllocation allocateImage(vk::ImageCreateInfo& imageCI, vk::Image& image);

        void freeImage(VmaAllocation alloc);

    private:
        VmaAllocator m_Allocator{};

        VmaVulkanFunctions m_VulkanFunctions{};
    };
}