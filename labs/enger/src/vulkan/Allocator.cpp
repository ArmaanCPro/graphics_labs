#include "Allocator.h"

#include "GpuResourceTypes.h"

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

    VulkanBuffer Allocator::createBuffer(vk::BufferCreateInfo &bufferCI,
                                         vk::MemoryPropertyFlags memFlags)
    {
        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_AUTO;

        if (memFlags & vk::MemoryPropertyFlagBits::eHostVisible)
        {
            allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            allocCI.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        }

        VmaAllocationInfo resultInfo{};
        VmaAllocation alloc{};
        VkBuffer rawBuf{};
        VkBufferCreateInfo& rawCI = bufferCI;
        vkCheck(vk::Result{
            vmaCreateBuffer(m_Allocator, &rawCI, &allocCI, &rawBuf, &alloc, &resultInfo)});

        VkMemoryPropertyFlags actualFlags;
        vmaGetAllocationMemoryProperties(m_Allocator, alloc, &actualFlags);

        return VulkanBuffer{
            .buffer_ = rawBuf,
            .allocation_ = alloc,
            .size_ = bufferCI.size,
            .mappedMemory_ = resultInfo.pMappedData,
            .isCoherent_ = (actualFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0,
        };
    }

    VulkanImage Allocator::createImage(vk::ImageCreateInfo& imageCI)
    {
        VmaAllocationCreateInfo allocCI{
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .priority = 1.0f, // keep this?
        };

        auto& ici = static_cast<VkImageCreateInfo&>(imageCI);
        VmaAllocation alloc{};
        VkImage rawImage{};
        vkCheck(vk::Result{vmaCreateImage(m_Allocator, &ici, &allocCI,
                           &rawImage, &alloc, nullptr)});

        return VulkanImage{
            .image_ = rawImage,
            .allocation_ = alloc,
            .extent_ = imageCI.extent,
            .usage_ = imageCI.usage,
            .format_ = imageCI.format,
        };
    }

    void Allocator::free(VmaAllocation alloc)
    {
        vmaFreeMemory(m_Allocator, alloc);
    }

    void Allocator::destroyBuffer(VmaAllocation alloc, vk::Buffer buffer)
    {
        assert(buffer != nullptr);
        vmaDestroyBuffer(m_Allocator, static_cast<VkBuffer>(buffer), alloc);
    }

    void Allocator::destroyImage(VmaAllocation alloc, vk::Image image)
    {
        assert(image != VK_NULL_HANDLE);
        vmaDestroyImage(m_Allocator, static_cast<VkImage>(image), alloc);
    }
}
