#include "GpuResourceTypes.h"

#include "Allocator.h"
#include "Logging/Assert.h"


namespace enger
{
    void VulkanBuffer::bufferSubData(const Allocator &allocator, size_t offset, size_t size, const void *data)
    {
        EASSERT(mappedMemory_ != nullptr, "Buffer is not mapped");

        EASSERT(offset + size <= size_, "Buffer subdata out of bounds");

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
        EASSERT(mappedMemory_ != nullptr, "Buffer is not mapped");

        EASSERT(offset + size <= size_, "Buffer subdata out of bounds");

        if (!isCoherent_)
        {
            invalidateMappedMemory(allocator, offset, size);
        }

        const uint8_t* src = static_cast<const uint8_t *>(mappedMemory_) + offset;
        memcpy(data, src, size);
    }

    void VulkanBuffer::flushMappedMemory(const Allocator &allocator, vk::DeviceSize offset, vk::DeviceSize size) const
    {
        EASSERT(mappedMemory_ != nullptr, "Buffer is not mapped");

        vmaFlushAllocation(allocator.allocator(), allocation_, offset, size);
    }

    void VulkanBuffer::invalidateMappedMemory(const Allocator &allocator, vk::DeviceSize offset,
        vk::DeviceSize size) const
    {
        EASSERT(mappedMemory_ != nullptr, "Buffer is not mapped");

        vmaInvalidateAllocation(allocator.allocator(), allocation_, offset, size);
    }
}
