#include "Allocator.h"

namespace enger
{
    Allocator::Allocator(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device)
    {
        VmaVulkanFunctions vkFunctions{
            .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr,
            .vkCreateBuffer = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateBuffer,
            .vkDestroyBuffer = VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyBuffer,
            .vkCreateImage = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateImage,
            .vkDestroyImage = VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyImage,
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
}
