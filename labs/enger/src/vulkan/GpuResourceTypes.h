#pragma once

#include "vk.h"

namespace enger
{
    struct VulkanImage final
    {
        vk::Image image_;
        vk::ImageView view_;
        VmaAllocation allocation_;
        vk::Extent3D extent_;
        vk::ImageUsageFlags usage_;
        vk::Format format_;
    };

    class Allocator;

    struct VulkanBuffer final
    {
        vk::Buffer buffer_{};
        VmaAllocation allocation_{};
        vk::DeviceAddress deviceAddress_{};
        vk::DeviceSize size_{};
        vk::BufferUsageFlags usage_{};
        vk::MemoryPropertyFlags memoryProperties_{};
        void* mappedMemory_ = nullptr;
        bool isCoherent_ = false;

        /// Upload data to a portion of memory.
        /// Only for host-visible buffers.
        void bufferSubData(const Allocator& allocator, size_t offset, size_t size, const void* data);
        /// Download data from GPU memory.
        /// Only for host-visible buffers.
        void getBufferSubData(const Allocator& allocator, size_t offset, size_t size, void* data);
        void flushMappedMemory(const Allocator& allocator, vk::DeviceSize offset, vk::DeviceSize size) const;
        void invalidateMappedMemory(const Allocator& allocator, vk::DeviceSize offset, vk::DeviceSize size) const;
    };
}