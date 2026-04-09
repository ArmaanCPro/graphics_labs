#include "Allocator.h"

namespace enger
{
    Allocator::Allocator(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device)
    {
        assert(instance != nullptr);
        assert(physicalDevice != nullptr);
        assert(device != nullptr);

        const auto& dld = VULKAN_HPP_DEFAULT_DISPATCHER;
        m_VulkanFunctions = {
            .vkGetInstanceProcAddr = dld.vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr   = dld.vkGetDeviceProcAddr,

            .vkGetPhysicalDeviceProperties       = dld.vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties = dld.vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory = dld.vkAllocateMemory,
            .vkFreeMemory = dld.vkFreeMemory,
            .vkMapMemory = dld.vkMapMemory,
            .vkUnmapMemory = dld.vkUnmapMemory,
            .vkFlushMappedMemoryRanges = dld.vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges = dld.vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory = dld.vkBindBufferMemory,
            .vkBindImageMemory = dld.vkBindImageMemory,
            .vkGetBufferMemoryRequirements = dld.vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements = dld.vkGetImageMemoryRequirements,
            .vkCreateBuffer = dld.vkCreateBuffer,
            .vkDestroyBuffer = dld.vkDestroyBuffer,
            .vkCreateImage = dld.vkCreateImage,
            .vkDestroyImage = dld.vkDestroyImage,
            .vkCmdCopyBuffer = dld.vkCmdCopyBuffer,

            .vkGetBufferMemoryRequirements2KHR = dld.vkGetBufferMemoryRequirements2,
            .vkGetImageMemoryRequirements2KHR  = dld.vkGetImageMemoryRequirements2,
            .vkBindBufferMemory2KHR = dld.vkBindBufferMemory2,
            .vkBindImageMemory2KHR  = dld.vkBindImageMemory2,

            .vkGetPhysicalDeviceMemoryProperties2KHR = dld.vkGetPhysicalDeviceMemoryProperties2,
        };
        VmaAllocatorCreateInfo allocatorCI{
            .flags = 0,//VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physicalDevice,
            .device = device,
            .pVulkanFunctions = &m_VulkanFunctions,
            .instance = instance,
        };
        vkCheck(vk::Result{vmaCreateAllocator(&allocatorCI, &m_Allocator)});
    }

    Allocator::~Allocator()
    {
        vmaDestroyAllocator(m_Allocator);
    }

    void Allocator::init(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device)
    {
        assert(instance != nullptr);
        assert(physicalDevice != nullptr);
        assert(device != nullptr);

        const auto& dld = VULKAN_HPP_DEFAULT_DISPATCHER;
        m_VulkanFunctions = {
            .vkGetInstanceProcAddr = dld.vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr   = dld.vkGetDeviceProcAddr,

            /*
            .vkGetPhysicalDeviceProperties       = dld.vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties = dld.vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory = dld.vkAllocateMemory,
            .vkFreeMemory = dld.vkFreeMemory,
            .vkMapMemory = dld.vkMapMemory,
            .vkUnmapMemory = dld.vkUnmapMemory,
            .vkFlushMappedMemoryRanges = dld.vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges = dld.vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory = dld.vkBindBufferMemory,
            .vkBindImageMemory = dld.vkBindImageMemory,
            .vkGetBufferMemoryRequirements = dld.vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements = dld.vkGetImageMemoryRequirements,
            .vkCreateBuffer = dld.vkCreateBuffer,
            .vkDestroyBuffer = dld.vkDestroyBuffer,
            .vkCreateImage = dld.vkCreateImage,
            .vkDestroyImage = dld.vkDestroyImage,
            .vkCmdCopyBuffer = dld.vkCmdCopyBuffer,
            */

            .vkGetBufferMemoryRequirements2KHR = dld.vkGetBufferMemoryRequirements2,
            .vkGetImageMemoryRequirements2KHR  = dld.vkGetImageMemoryRequirements2,
            .vkBindBufferMemory2KHR = dld.vkBindBufferMemory2,
            .vkBindImageMemory2KHR  = dld.vkBindImageMemory2,

            .vkGetPhysicalDeviceMemoryProperties2KHR = dld.vkGetPhysicalDeviceMemoryProperties2,
        };
        VmaAllocatorCreateInfo allocatorCI{
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physicalDevice,
            .device = device,
            .pVulkanFunctions = &m_VulkanFunctions,
            .instance = instance,
        };
        vkCheck(vk::Result{vmaCreateAllocator(&allocatorCI, &m_Allocator)});
    }

    VmaAllocation Allocator::allocateImage(vk::ImageCreateInfo& imageCI, vk::Image& image)
    {
        VmaAllocationCreateInfo allocCI{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        VmaAllocation alloc{};

        VkImageCreateInfo& ici = static_cast<VkImageCreateInfo&>(imageCI);
        VkImage rawImage{};

        vkCheck(vk::Result{vmaCreateImage(m_Allocator, &ici, &allocCI,
                           &rawImage, &alloc, nullptr)});

        image = rawImage;

        return alloc;
    }

    void Allocator::freeImage(VmaAllocation alloc)
    {
        //assert(image != VK_NULL_HANDLE);
        //vmaDestroyImage(m_Allocator, static_cast<VkImage>(image), alloc);
        vmaFreeMemory(m_Allocator, alloc);
    }
}
