#pragma once

#include "vk.h"
#include "Utils/InplaceVector.h"

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
        LOG_ERROR("Unknown BPP for format: {}", vk::to_string(format));
        return 0;
    }

    // Used for pre-generated Mipmaps.
    struct TextureSubresource
    {
        const void* data = nullptr;
        vk::Extent3D extent = {1, 1, 1}; // Possibly to calculate manually, but libktx provides it anyway (better for non-Po2 extent)
        uint32_t mipLevel = 0;
        uint32_t arrayLayer = 0;
        size_t size = 0;
    };

    // Not an actual GPU resource type, but I don't know where else to put it. Used for texture creation as a parameter to device
    struct TextureDesc
    {
        vk::ImageType type = vk::ImageType::e2D;
        vk::Format format = vk::Format::eUndefined;

        // Specifies the allocation dimensions. Not to be confused with TextureSubresource::Extent, which is upload dimensions (can be the same).
        vk::Extent3D dimensions = {1, 1, 1};
        uint32_t mipLevels = 1; // cannot be used in conjunctino with generateMipMaps, as partial mip chains are not supported.
        uint32_t arrayLayers = 1;
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
        vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;

        bool generateMipmaps = false;

        static constexpr auto kMaxSubresources = 128uz;
        /// Data payload
        /// There should be mipLevels * arrayLayers count subresources. Describes mip maps for each layer.
        InplaceVector<TextureSubresource, kMaxSubresources> subresources;
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

    struct ImageState
    {
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
        vk::AccessFlags2 access = vk::AccessFlagBits2::eMemoryWrite;
        vk::PipelineStageFlags2 stage = vk::PipelineStageFlagBits2::eAllCommands;
    };

    // Helpers — derive required ImageState from usage

    inline ImageState colorWriteState() {
        return {
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
            .access = vk::AccessFlagBits2::eColorAttachmentWrite,
            .stage  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        };
    }

    inline ImageState depthWriteState() {
        return {
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite
                    | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
            .stage  = vk::PipelineStageFlagBits2::eEarlyFragmentTests
                    | vk::PipelineStageFlagBits2::eLateFragmentTests,
        };
    }

    inline ImageState shaderReadState() {
        return {
            .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .access = vk::AccessFlagBits2::eShaderRead,
            .stage  = vk::PipelineStageFlagBits2::eFragmentShader,
        };
    }

    inline ImageState transferSrcState() {
        return {
            .layout = vk::ImageLayout::eTransferSrcOptimal,
            .access = vk::AccessFlagBits2::eTransferRead,
            .stage  = vk::PipelineStageFlagBits2::eTransfer,
        };
    }

    inline ImageState transferDstState() {
        return {
            .layout = vk::ImageLayout::eTransferDstOptimal,
            .access = vk::AccessFlagBits2::eTransferWrite,
            .stage  = vk::PipelineStageFlagBits2::eTransfer,
        };
    }

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
        ImageState state_;

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
