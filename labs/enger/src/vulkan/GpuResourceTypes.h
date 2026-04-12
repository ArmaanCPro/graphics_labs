#pragma once

#include "vk.h"

namespace enger
{
    struct VulkanImage final
    {
        // clang-format off
        [[nodiscard]] inline bool isSampledImage() const { return (usage_  & vk::ImageUsageFlagBits::eSampled) == vk::ImageUsageFlagBits::eSampled; }
        [[nodiscard]] inline bool isStorageImage() const { return (usage_  & vk::ImageUsageFlagBits::eStorage) == vk::ImageUsageFlagBits::eStorage; }
        [[nodiscard]] inline bool isColorAttachment() const { return (usage_ & vk::ImageUsageFlagBits::eColorAttachment) == vk::ImageUsageFlagBits::eColorAttachment; }
        [[nodiscard]] inline bool isDepthAttachment() const { return (usage_ & vk::ImageUsageFlagBits::eDepthStencilAttachment) == vk::ImageUsageFlagBits::eDepthStencilAttachment; }
        [[nodiscard]] inline bool isTransferSrc() const { return (usage_  & vk::ImageUsageFlagBits::eTransferSrc) == vk::ImageUsageFlagBits::eTransferSrc; }
        [[nodiscard]] inline bool isTransferDst() const { return (usage_  & vk::ImageUsageFlagBits::eTransferDst) == vk::ImageUsageFlagBits::eTransferDst; }
        // clang-format on

        vk::Image image_;
        vk::ImageView view_;
        VmaAllocation allocation_;

        vk::Extent3D extent_;
        vk::ImageUsageFlags usage_;
        vk::Format format_;
        vk::ImageAspectFlags aspectFlags_;

        // for Combined Image Samplers
        // vk::Sampler sampler_{};
    };

    class Allocator;

    struct VulkanBuffer final
    {
        vk::Buffer buffer_{};
        VmaAllocation allocation_{};

        vk::DeviceAddress deviceAddress_{};
        vk::DeviceSize size_{};

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