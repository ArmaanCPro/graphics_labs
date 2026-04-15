#pragma once

#include "vk.h"

namespace enger
{
    // helper function
    constexpr uint32_t findBppFromFormat(vk::Format format)
    {
        switch (format)
        {
            case vk::Format::eR8G8B8A8Unorm:
            case vk::Format::eR8G8B8A8Srgb: return 32;
            case vk::Format::eR16G16B16A16Sfloat: return 64;
            default:
                break;
        }
        std::cerr << "Unknown BPP for format: " << vk::to_string(format) << std::endl;
        return 0;
    }

    // Not an actual GPU resource type, but I don't know where else to put it. Used for texture creation as a parameter to device
    struct TextureDesc
    {
        vk::ImageType type = vk::ImageType::e2D;
        vk::Format format = vk::Format::eUndefined;

        vk::Extent3D dimensions = {1, 1, 1};
        uint32_t arrayLayers = 1;
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
        vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        uint32_t mipLevels = 1;
        vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        const void* initialData = nullptr;
    };

    struct SamplerDesc
    {
        vk::Filter magFilter = vk::Filter::eLinear;
        vk::Filter minFilter = vk::Filter::eLinear;
        vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear;
        vk::SamplerAddressMode addressModeU = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode addressModeV = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode addressModeW = vk::SamplerAddressMode::eRepeat;
        bool anisotropyEnable = true;
        float maxAnisotropy = 16.0f; // maybe consider dropping to 8.0f as the default
        float minLod = 0.0f;
        float maxLod = vk::LodClampNone;
    };

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

        void* mappedMemory_ = nullptr;

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