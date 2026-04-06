#pragma once

#include "vk.h"

#include <vk_mem_alloc.h>

namespace enger
{
    class Allocator
    {
    public:
        Allocator(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);

        VmaAllocation allocateBuffer() { return {}; }
        VmaAllocation allocateImage() { return {}; }

    private:
        VmaAllocator m_Allocator;
    };
}