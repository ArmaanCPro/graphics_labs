#pragma once

#include "vk.h"

#include "Resources.h"

namespace enger
{
    // forward decl
    class Device;

    /// Used to allocate Descriptor Sets from Device.
    struct DescriptorSetLayoutDesc
    {
        std::span<uint32_t> bindIndices;
        std::span<vk::DescriptorType> types;
        vk::ShaderStageFlags shaderStages;
        void* pNext = nullptr;
        vk::DescriptorSetLayoutCreateFlags flags = {};
    };

    /// A long-lived Pool for allocating Descriptor Sets. A helper struct.
    /// TODO cleanup API and remove raw vk::Device parameters.
    struct DescriptorAllocator
    {
        struct PoolSizeRatio
        {
            vk::DescriptorType type_;
            float ratio_;
        };

        vk::DescriptorPool pool_;

        void initPool(vk::Device device, uint32_t maxSets, std::span<PoolSizeRatio> poolSizeRatios);
        void clearDescriptors(vk::Device device);
        void destroyPool(vk::Device device);

        [[nodiscard]] vk::DescriptorSet allocate(Device& device, DescriptorSetLayoutHandle handle);
    };
}
