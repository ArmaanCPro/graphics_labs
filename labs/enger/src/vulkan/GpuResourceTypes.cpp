#include "GpuResourceTypes.h"

#include "Allocator.h"

namespace enger
{
    void VulkanBuffer::bufferSubData(const Allocator &allocator, size_t offset, size_t size, const void *data)
    {
        assert(mappedMemory_ != nullptr);

        assert(offset + size <= size_);

        if (data)
        {
            memcpy(static_cast<uint8_t *>(mappedMemory_) + offset, data, size);
        }
        else
        {
            memset(static_cast<uint8_t *>(mappedMemory_) + offset, 0, size);
        }

        if (!isCoherent_)
        {
            flushMappedMemory(allocator, offset, size);
        }
    }

    void VulkanBuffer::getBufferSubData(const Allocator &allocator, size_t offset, size_t size, void *data)
    {
        assert(mappedMemory_ != nullptr);

        assert(offset + size <= size_);

        if (!isCoherent_)
        {
            invalidateMappedMemory(allocator, offset, size);
        }

        const uint8_t* src = static_cast<const uint8_t *>(mappedMemory_) + offset;
        memcpy(data, src, size);
    }

    void VulkanBuffer::flushMappedMemory(const Allocator &allocator, vk::DeviceSize offset, vk::DeviceSize size) const
    {
        assert(mappedMemory_ != nullptr);

        vmaFlushAllocation(allocator.allocator(), allocation_, offset, size);
    }

    void VulkanBuffer::invalidateMappedMemory(const Allocator &allocator, vk::DeviceSize offset,
        vk::DeviceSize size) const
    {
        assert(mappedMemory_ != nullptr);

        vmaInvalidateAllocation(allocator.allocator(), allocation_, offset, size);
    }
}
