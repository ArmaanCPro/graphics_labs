#include "Allocator.h"

namespace enger
{
    Allocator::Allocator(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device)
    {
        const auto& dld = VULKAN_HPP_DEFAULT_DISPATCHER;

        VmaVulkanFunctions vkFunctions{
            .vkGetInstanceProcAddr = dld.vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = dld.vkGetDeviceProcAddr,
        };
        VmaAllocatorCreateInfo allocatorCI{
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physicalDevice,
            .device = device,
            .pVulkanFunctions = &vkFunctions,
            .instance = instance,
        };
        vkCheck(vk::Result{vmaCreateAllocator(&allocatorCI, &m_Allocator)});
    }

    Allocator::~Allocator()
    {
        vmaDestroyAllocator(m_Allocator);
    }
}
